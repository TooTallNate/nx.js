#include "download.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include "match.h"

// ---------------------------------------------------------------------------
// Minimal HTTPS client over the system `ssl` service (TLS + cert verification +
// SNI handled natively) layered on BSD sockets. Just enough HTTP/1.1 to GET the
// GitHub releases API and download a release asset (following one redirect).
// ---------------------------------------------------------------------------

#define DL_USER_AGENT "nx.js-bootstrap"

// One open TLS connection (socket + ssl conn). The ssl service owns the socket
// (DoNotCloseSocket not set), so sslConnectionClose closes it for us.
typedef struct {
	int sockfd;
	SslContext ctx;
	SslConnection conn;
	bool have_ctx;
	bool have_conn;
} https_t;

static void https_close(https_t *h) {
	if (h->have_conn) {
		sslConnectionClose(&h->conn);
		h->have_conn = false;
	}
	if (h->have_ctx) {
		sslContextClose(&h->ctx);
		h->have_ctx = false;
	}
	// sslConnectionClose closes the socket (we don't set DoNotCloseSocket); only
	// close here if the connection was never established.
	if (h->sockfd >= 0) {
		// If a conn was created over it, ssl already closed it; guard with a
		// flag so we don't double-close.
		h->sockfd = -1;
	}
}

// Resolve `host` and open a TCP connection to port 443.
static int tcp_connect(const char *host) {
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET; // Switch network stack is IPv4
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo *res = NULL;
	if (getaddrinfo(host, "443", &hints, &res) != 0 || !res)
		return -1;
	int fd = -1;
	for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0)
			continue;
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	return fd;
}

// Open a TLS connection to `host`:443. Returns true on success.
static bool https_open(https_t *h, const char *host) {
	memset(h, 0, sizeof(*h));
	h->sockfd = -1;

	h->sockfd = tcp_connect(host);
	if (h->sockfd < 0)
		return false;

	if (R_FAILED(sslCreateContext(&h->ctx, SslVersion_Auto)))
		return false;
	h->have_ctx = true;
	if (R_FAILED(sslContextCreateConnection(&h->ctx, &h->conn)))
		return false;
	h->have_conn = true;

	// Verify peer CA + hostname (the default), set SNI, hand over the socket.
	// IMPORTANT: use the libnx socket WRAPPER socketSslConnectionSetSocketDescriptor,
	// NOT the raw sslConnectionSetSocketDescriptor IPC cmd — the wrapper
	// registers/duplicates the BSD fd through the bsdsocket layer for the ssl
	// service. The raw cmd fails (0xe87b) with a plain socket() fd.
	sslConnectionSetHostName(&h->conn, host, strlen(host));
	int out_sockfd = socketSslConnectionSetSocketDescriptor(&h->conn, h->sockfd);
	if (out_sockfd < 0 && errno != ENOENT)
		return false;
	h->sockfd = -1; // owned by the ssl connection now

	u32 out_size = 0, total_certs = 0;
	if (R_FAILED(sslConnectionDoHandshake(&h->conn, &out_size, &total_certs,
	                                      NULL, 0)))
		return false;
	return true;
}

static bool https_write_all(https_t *h, const char *buf, size_t len) {
	size_t off = 0;
	while (off < len) {
		u32 wrote = 0;
		Result rc =
		    sslConnectionWrite(&h->conn, buf + off, (u32)(len - off), &wrote);
		if (R_FAILED(rc) || wrote == 0)
			return false;
		off += wrote;
	}
	return true;
}

// Read up to `cap` bytes; returns the count (0 = clean EOF, -1 = error).
static int https_read(https_t *h, void *buf, u32 cap) {
	u32 got = 0;
	Result rc = sslConnectionRead(&h->conn, buf, cap, &got);
	if (R_FAILED(rc))
		return -1;
	return (int)got;
}

// Send a GET request for `path` to the already-open connection `host`.
static bool send_get(https_t *h, const char *host, const char *path) {
	char req[1024];
	int n = snprintf(req, sizeof(req),
	                 "GET %s HTTP/1.1\r\n"
	                 "Host: %s\r\n"
	                 "User-Agent: " DL_USER_AGENT "\r\n"
	                 "Accept: */*\r\n"
	                 "Connection: close\r\n"
	                 "\r\n",
	                 path, host);
	if (n <= 0 || (size_t)n >= sizeof(req))
		return false;
	return https_write_all(h, req, (size_t)n);
}

// ---------------------------------------------------------------------------
// HTTP response handling.
// ---------------------------------------------------------------------------

// Parsed response head: status code + (optional) Location for redirects +
// Content-Length. The body bytes already read past the header are returned in
// `leftover`/`leftover_len` (the caller continues reading from the connection).
typedef struct {
	int status;
	char location[1024];
	long content_length; // -1 if unknown
} resp_head_t;

// Case-insensitive header prefix check; returns the value (trimmed) or NULL.
static const char *header_value(const char *line, const char *name) {
	size_t nl = strlen(name);
	if (strncasecmp(line, name, nl) != 0)
		return NULL;
	const char *p = line + nl;
	if (*p != ':')
		return NULL;
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

// Read + parse the response head (status line + headers) up to the blank line.
// `prebuf`/`*prebuf_len` is a scratch buffer; on return it holds any body bytes
// read past the header (`*prebuf_len` updated). Returns false on error.
static bool read_response_head(https_t *h, resp_head_t *out, char *prebuf,
                               size_t prebuf_cap, size_t *prebuf_len) {
	memset(out, 0, sizeof(*out));
	out->status = 0;
	out->content_length = -1;

	// Accumulate until we see the end-of-headers CRLFCRLF.
	size_t len = 0;
	const char *hdr_end = NULL;
	while (len < prebuf_cap - 1) {
		int got = https_read(h, prebuf + len, (u32)(prebuf_cap - 1 - len));
		if (got < 0)
			return false;
		if (got == 0)
			break; // EOF before full header
		len += (size_t)got;
		prebuf[len] = '\0';
		hdr_end = strstr(prebuf, "\r\n\r\n");
		if (hdr_end)
			break;
	}
	if (!hdr_end)
		return false;

	// Status line: "HTTP/1.1 NNN ...".
	const char *sp = strchr(prebuf, ' ');
	if (!sp)
		return false;
	out->status = atoi(sp + 1);

	// Walk header lines.
	const char *line = strchr(prebuf, '\n');
	if (line)
		line++;
	while (line && line < hdr_end) {
		const char *eol = strchr(line, '\n');
		if (!eol || eol > hdr_end)
			break;
		// (line .. eol) is one header line (may end with \r).
		const char *v;
		if ((v = header_value(line, "Location"))) {
			size_t i = 0;
			while (v < eol && *v != '\r' && *v != '\n' &&
			       i < sizeof(out->location) - 1)
				out->location[i++] = *v++;
			out->location[i] = '\0';
		} else if ((v = header_value(line, "Content-Length"))) {
			out->content_length = atol(v);
		}
		line = eol + 1;
	}

	// Body bytes already read = everything after the CRLFCRLF.
	const char *body = hdr_end + 4;
	size_t body_len = len - (size_t)(body - prebuf);
	memmove(prebuf, body, body_len);
	*prebuf_len = body_len;
	return true;
}

// Split an https URL into host + path. Returns false if not https.
static bool split_url(const char *url, char *host, size_t host_sz, char *path,
                      size_t path_sz) {
	const char *p = url;
	if (strncasecmp(p, "https://", 8) != 0)
		return false;
	p += 8;
	const char *slash = strchr(p, '/');
	size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
	if (hlen == 0 || hlen >= host_sz)
		return false;
	memcpy(host, p, hlen);
	host[hlen] = '\0';
	if (slash)
		snprintf(path, path_sz, "%s", slash);
	else
		snprintf(path, path_sz, "/");
	return true;
}

// ---------------------------------------------------------------------------
// GitHub releases: find the highest release tag satisfying the requirement.
// ---------------------------------------------------------------------------

// Scrape `"tag_name":"v<version>"` occurrences from the releases JSON, keeping
// the highest version that satisfies `spec`. Writes the bare version (no `v`)
// into `out_ver`. Returns true if a satisfying release was found.
static bool pick_release(const char *json, const char *spec, char *out_ver,
                         size_t out_ver_sz) {
	bool found = false;
	char best[128] = {0};
	const char *p = json;
	const char *key = "\"tag_name\"";
	while ((p = strstr(p, key)) != NULL) {
		p += strlen(key);
		// skip spaces + ':'
		while (*p == ' ' || *p == '\t' || *p == ':')
			p++;
		if (*p != '"') {
			continue;
		}
		p++; // opening quote
		// value, e.g. v1.0.0-beta.2
		char tag[128];
		size_t i = 0;
		while (*p && *p != '"' && i < sizeof(tag) - 1)
			tag[i++] = *p++;
		tag[i] = '\0';
		const char *ver = tag;
		if (ver[0] == 'v' || ver[0] == 'V')
			ver++;
		if (!nx_version_satisfies(ver, spec))
			continue;
		if (!found || nx_version_compare(ver, best) > 0) {
			snprintf(best, sizeof(best), "%s", ver);
			found = true;
		}
	}
	if (found)
		snprintf(out_ver, out_ver_sz, "%s", best);
	return found;
}

// GET an HTTPS URL and return the full response body in a malloc'd buffer
// (NUL-terminated). Follows up to `max_redirects` 3xx Location hops. Caller
// frees `*out`. `*out_len` is the body length. Returns false on error.
static bool http_get_all(const char *url, int max_redirects, char **out,
                         size_t *out_len) {
	char cur[1200];
	snprintf(cur, sizeof(cur), "%s", url);
	for (int redir = 0; redir <= max_redirects; redir++) {
		char host[256], path[1024];
		if (!split_url(cur, host, sizeof(host), path, sizeof(path)))
			return false;
		https_t h;
		if (!https_open(&h, host)) {
			https_close(&h);
			return false;
		}
		if (!send_get(&h, host, path)) {
			https_close(&h);
			return false;
		}
		static char scratch[8192];
		size_t pre_len = 0;
		resp_head_t head;
		if (!read_response_head(&h, &head, scratch, sizeof(scratch),
		                        &pre_len)) {
			https_close(&h);
			return false;
		}
		if (head.status >= 300 && head.status < 400 && head.location[0]) {
			https_close(&h);
			snprintf(cur, sizeof(cur), "%s", head.location);
			continue;
		}
		if (head.status != 200) {
			https_close(&h);
			return false;
		}
		// Read the body: prebuffered bytes + the rest of the stream.
		size_t cap = head.content_length > 0
		                 ? (size_t)head.content_length + 1
		                 : 64 * 1024;
		char *buf = (char *)malloc(cap);
		if (!buf) {
			https_close(&h);
			return false;
		}
		size_t len = 0;
		memcpy(buf, scratch, pre_len);
		len = pre_len;
		for (;;) {
			if (len + 4096 > cap) {
				size_t ncap = cap * 2;
				char *nb = (char *)realloc(buf, ncap);
				if (!nb) {
					free(buf);
					https_close(&h);
					return false;
				}
				buf = nb;
				cap = ncap;
			}
			int got = https_read(&h, buf + len, 4096);
			if (got < 0) {
				free(buf);
				https_close(&h);
				return false;
			}
			if (got == 0)
				break;
			len += (size_t)got;
		}
		buf[len] = '\0';
		https_close(&h);
		*out = buf;
		*out_len = len;
		return true;
	}
	return false; // too many redirects
}

// ---------------------------------------------------------------------------
// Asset download (streamed to a file, with progress).
// ---------------------------------------------------------------------------

static void print_progress(long done, long total) {
	if (total > 0) {
		int pct = (int)((done * 100) / total);
		printf("\r  downloading... %d%% (%ld / %ld KiB)   ", pct, done / 1024,
		       total / 1024);
	} else {
		printf("\r  downloading... %ld KiB   ", done / 1024);
	}
	consoleUpdate(NULL);
}

// GET `url` (following redirects) and stream the body to `dest_path`, rendering
// progress. Returns false on any error (and removes the partial file).
static bool download_to_file(const char *url, const char *dest_path) {
	char cur[1200];
	snprintf(cur, sizeof(cur), "%s", url);
	for (int redir = 0; redir <= 5; redir++) {
		char host[256], path[1024];
		if (!split_url(cur, host, sizeof(host), path, sizeof(path)))
			return false;
		https_t h;
		if (!https_open(&h, host)) {
			https_close(&h);
			return false;
		}
		if (!send_get(&h, host, path)) {
			https_close(&h);
			return false;
		}
		static char scratch[8192];
		size_t pre_len = 0;
		resp_head_t head;
		if (!read_response_head(&h, &head, scratch, sizeof(scratch),
		                        &pre_len)) {
			https_close(&h);
			return false;
		}
		if (head.status >= 300 && head.status < 400 && head.location[0]) {
			https_close(&h);
			snprintf(cur, sizeof(cur), "%s", head.location);
			continue;
		}
		if (head.status != 200) {
			https_close(&h);
			printf("\n  server returned HTTP %d\n", head.status);
			return false;
		}
		FILE *f = fopen(dest_path, "wb");
		if (!f) {
			https_close(&h);
			return false;
		}
		long total = head.content_length;
		long done = 0;
		bool ok = true;
		if (pre_len) {
			if (fwrite(scratch, 1, pre_len, f) != pre_len)
				ok = false;
			done += (long)pre_len;
		}
		static char chunk[16384];
		while (ok) {
			int got = https_read(&h, chunk, sizeof(chunk));
			if (got < 0) {
				ok = false;
				break;
			}
			if (got == 0)
				break;
			if (fwrite(chunk, 1, (size_t)got, f) != (size_t)got) {
				ok = false;
				break;
			}
			done += got;
			print_progress(done, total);
		}
		print_progress(done, total);
		printf("\n");
		fclose(f);
		https_close(&h);
		if (!ok || (total > 0 && done != total)) {
			remove(dest_path);
			return false;
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Service bring-up.
// ---------------------------------------------------------------------------

static bool net_up = false;

static bool ensure_network(void) {
	if (net_up)
		return true;
	if (R_FAILED(nifmInitialize(NifmServiceType_User)))
		return false;
	NifmInternetConnectionType type;
	u32 strength;
	NifmInternetConnectionStatus status;
	if (R_FAILED(nifmGetInternetConnectionStatus(&type, &strength, &status)) ||
	    status != NifmInternetConnectionStatus_Connected) {
		nifmExit();
		return false;
	}
	// Socket + ssl services. Use a LEAN socket config: the launcher only needs a
	// couple of short-lived TLS connections, and the forwarder runs with a tiny
	// (16 MiB) malloc heap + small memory grant, so the default's large
	// transfer-memory reservation is both unnecessary and risky here.
	static const SocketInitConfig sock_cfg = {
	    .tcp_tx_buf_size = 0x8000,
	    .tcp_rx_buf_size = 0x10000,
	    .tcp_tx_buf_max_size = 0x20000,
	    .tcp_rx_buf_max_size = 0x40000,
	    .udp_tx_buf_size = 0x2400,
	    .udp_rx_buf_size = 0xA500,
	    .sb_efficiency = 2,
	    .num_bsd_sessions = 2,
	    .bsd_service_type = BsdServiceType_User,
	};
	if (R_FAILED(socketInitialize(&sock_cfg))) {
		nifmExit();
		return false;
	}
	if (R_FAILED(sslInitialize(2))) {
		socketExit();
		nifmExit();
		return false;
	}
	net_up = true;
	return true;
}

static void teardown_network(void) {
	if (!net_up)
		return;
	sslExit();
	socketExit();
	nifmExit();
	net_up = false;
}

// ---------------------------------------------------------------------------
// Public entry point.
// ---------------------------------------------------------------------------

bool nx_download_runtime(nx_resolve_t *r) {
	printf("Downloading a compatible nx.js runtime...\n");
	printf("  requirement: %s\n", r->version);
	consoleUpdate(NULL);

	if (!ensure_network()) {
		printf("  no internet connection available.\n");
		consoleUpdate(NULL);
		return false;
	}

	bool result = false;

	// 1. Query the releases API and pick the best satisfying tag.
	printf("  querying GitHub releases...\n");
	consoleUpdate(NULL);
	char *json = NULL;
	size_t json_len = 0;
	if (!http_get_all("https://api.github.com/repos/" NXJS_GH_REPO
	                  "/releases?per_page=100",
	                  5, &json, &json_len)) {
		printf("  could not reach the GitHub releases API.\n");
		consoleUpdate(NULL);
		teardown_network();
		return false;
	}
	char ver[128] = {0};
	bool picked = pick_release(json, r->version, ver, sizeof(ver));
	free(json);
	if (!picked) {
		printf("  no published release satisfies \"%s\".\n", r->version);
		consoleUpdate(NULL);
		teardown_network();
		return false;
	}
	printf("  selected: v%s\n", ver);
	consoleUpdate(NULL);

	// 2. Download the asset to a temp file, then rename into place.
	char url[512];
	snprintf(url, sizeof(url),
	         "https://github.com/" NXJS_GH_REPO "/releases/download/v%s/" NXJS_RELEASE_ASSET,
	         ver);
	char tmp_path[FS_MAX_PATH];
	char final_path[FS_MAX_PATH];
	snprintf(final_path, sizeof(final_path),
	         NXJS_RUNTIME_DIR "/" NXJS_RUNTIME_PREFIX "%s" NXJS_RUNTIME_SUFFIX,
	         ver);
	snprintf(tmp_path, sizeof(tmp_path),
	         NXJS_RUNTIME_DIR "/" NXJS_RUNTIME_PREFIX "%s" NXJS_RUNTIME_SUFFIX
	                          ".part",
	         ver);

	// Ensure the runtime dir exists.
	mkdir(NXJS_RUNTIME_DIR, 0777);

	if (!download_to_file(url, tmp_path)) {
		printf("  download failed.\n");
		consoleUpdate(NULL);
		teardown_network();
		return false;
	}
	remove(final_path); // in case a stale/partial one exists
	if (rename(tmp_path, final_path) != 0) {
		printf("  could not save the runtime.\n");
		consoleUpdate(NULL);
		remove(tmp_path);
		teardown_network();
		return false;
	}

	printf("  installed %s\n", final_path);
	consoleUpdate(NULL);
	snprintf(r->runtime_path, sizeof(r->runtime_path), "%s", final_path);
	snprintf(r->runtime_version, sizeof(r->runtime_version), "%s", ver);
	result = true;

	teardown_network();
	return result;
}
