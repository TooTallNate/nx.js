#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define INI_IMPLEMENTATION
#include "vendor/ini.h"

// ---------------------------------------------------------------------------
// Logging helper: every value that can't be honored prints a `[config]` line
// to stderr (redirected to sdmc:/switch/nxjs-debug.log on device).
// ---------------------------------------------------------------------------
static void cfg_log(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	fputs("[config] ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	fflush(stderr);
}

// ---------------------------------------------------------------------------
// Value parsers.
// ---------------------------------------------------------------------------

static bool str_ieq(const char *a, const char *b) {
	return strcasecmp(a, b) == 0;
}

// Parse a size string: decimal digits with optional KiB/MiB/GiB/KB/MB/GB/K/M/G
// suffix (case-insensitive), or raw bytes. Returns true on success. Detects
// both strtoull range overflow and overflow of the value*multiplier product so
// a huge value can't silently wrap to a small (apparently valid) size.
static bool parse_size(const char *s, uint64_t *out) {
	if (!s || !*s)
		return false;
	errno = 0;
	char *end = NULL;
	unsigned long long v = strtoull(s, &end, 10);
	if (end == s)
		return false; // no digits
	if (errno == ERANGE)
		return false; // value itself overflowed uint64
	while (*end == ' ' || *end == '\t')
		end++;
	uint64_t mult = 1;
	if (*end) {
		if (str_ieq(end, "k") || str_ieq(end, "kb") || str_ieq(end, "kib"))
			mult = 1024ull;
		else if (str_ieq(end, "m") || str_ieq(end, "mb") || str_ieq(end, "mib"))
			mult = 1024ull * 1024;
		else if (str_ieq(end, "g") || str_ieq(end, "gb") || str_ieq(end, "gib"))
			mult = 1024ull * 1024 * 1024;
		else
			return false; // unknown suffix
	}
	// Guard the multiply against uint64 overflow.
	if (mult != 0 && (uint64_t)v > UINT64_MAX / mult)
		return false;
	*out = (uint64_t)v * mult;
	return true;
}

static bool parse_u32(const char *s, uint32_t *out) {
	uint64_t v;
	if (!parse_size(s, &v))
		return false;
	if (v > 0xFFFFFFFFull)
		return false;
	*out = (uint32_t)v;
	return true;
}

// Parse a finite, non-negative decimal (e.g. "22", "1.25", "0.5").
static bool parse_double(const char *s, double *out) {
	if (!s || !*s)
		return false;
	errno = 0;
	char *end = NULL;
	double v = strtod(s, &end);
	if (end == s || errno == ERANGE)
		return false;
	while (*end == ' ' || *end == '\t')
		end++;
	if (*end)
		return false; // trailing garbage
	if (!(v >= 0.0) || v != v) // reject negatives, NaN, inf
		return false;
	*out = v;
	return true;
}

// Replace a strdup'd color string slot with a copy of `value`.
static void set_color(char **slot, const char *value) {
	free(*slot);
	*slot = strdup(value);
}

// ANSI palette keys (indices 0-15), matching nx_console_config_t.ansi[].
static const char *const CONSOLE_ANSI_KEYS[16] = {
	"black", "red", "green", "yellow",
	"blue", "magenta", "cyan", "white",
	"bright_black", "bright_red", "bright_green", "bright_yellow",
	"bright_blue", "bright_magenta", "bright_cyan", "bright_white",
};

// ---------------------------------------------------------------------------
// inih handler.
// ---------------------------------------------------------------------------

static int ini_cb(void *user, const char *section, const char *name,
                  const char *value) {
	nx_config_t *cfg = (nx_config_t *)user;
	if (!section || !name || !value)
		return 1;

	if (str_ieq(section, "v8")) {
		if (str_ieq(name, "jit")) {
			if (str_ieq(value, "auto"))
				cfg->jit = NX_JIT_AUTO;
			else if (str_ieq(value, "on") || str_ieq(value, "true") ||
			         str_ieq(value, "1"))
				cfg->jit = NX_JIT_ON;
			else if (str_ieq(value, "off") || str_ieq(value, "false") ||
			         str_ieq(value, "0"))
				cfg->jit = NX_JIT_OFF;
			else
				cfg_log("v8.jit=\"%s\" not honored: invalid (use auto|on|off), "
				        "using auto",
				        value);
		} else if (str_ieq(name, "flags")) {
			free(cfg->v8_flags);
			cfg->v8_flags = strdup(value);
		} else if (str_ieq(name, "code_headroom_mb") ||
		           str_ieq(name, "wasm")) {
			// `wasm = on/off` is sugar for code_headroom_mb = 64 / 0.
			if (str_ieq(name, "wasm")) {
				if (str_ieq(value, "on") || str_ieq(value, "true") ||
				    str_ieq(value, "1"))
					cfg->code_headroom_mb = 64;
				else if (str_ieq(value, "off") || str_ieq(value, "false") ||
				         str_ieq(value, "0"))
					cfg->code_headroom_mb = 0;
				else
					cfg_log("v8.wasm=\"%s\" not honored: invalid (use on|off)",
					        value);
			} else {
				char *end = NULL;
				unsigned long mb = strtoul(value, &end, 10);
				if (end && *end == '\0' && mb <= 1024)
					cfg->code_headroom_mb = (uint32_t)mb;
				else
					cfg_log("v8.code_headroom_mb=\"%s\" not honored: invalid "
					        "(use 0-1024)",
					        value);
			}
		} else {
			cfg_log("v8.%s ignored: unknown key", name);
		}
		return 1;
	}

	if (str_ieq(section, "memory")) {
		if (str_ieq(name, "heap_limit")) {
			uint64_t v;
			if (parse_size(value, &v))
				cfg->heap_limit = v;
			else
				cfg_log("memory.heap_limit=\"%s\" not honored: invalid size, "
				        "using computed default",
				        value);
		} else {
			cfg_log("memory.%s ignored: unknown key", name);
		}
		return 1;
	}

	if (str_ieq(section, "renderer")) {
		if (str_ieq(name, "mode")) {
			if (str_ieq(value, "auto"))
				cfg->renderer = NX_RENDER_AUTO;
			else if (str_ieq(value, "cpu") || str_ieq(value, "raster"))
				cfg->renderer = NX_RENDER_CPU;
			else if (str_ieq(value, "gpu"))
				cfg->renderer = NX_RENDER_GPU;
			else
				cfg_log("renderer.mode=\"%s\" not honored: invalid "
				        "(use auto|cpu|gpu), using auto",
				        value);
		} else if (str_ieq(name, "gpu_cache")) {
			// GPU resource-cache budget in MiB. "auto" (default) = regime
			// pick; "default" = force Skia's own default (encoded as 0); a
			// number = explicit MiB cap. See NX_GPU_CACHE_AUTO in config.h.
			if (str_ieq(value, "auto")) {
				cfg->gpu_cache_mib = NX_GPU_CACHE_AUTO;
			} else if (str_ieq(value, "default")) {
				cfg->gpu_cache_mib = 0;
			} else {
				char *end = NULL;
				unsigned long mb = strtoul(value, &end, 10);
				// `end != value` requires at least one digit consumed: an
				// empty value (`gpu_cache =`) would otherwise parse as 0 and
				// silently force Skia's default instead of being reported.
				if (end && end != value && *end == '\0' && mb <= 4096)
					cfg->gpu_cache_mib = (uint32_t)mb;
				else
					cfg_log("renderer.gpu_cache=\"%s\" not honored: invalid "
					        "(use auto|default|0-4096)",
					        value);
			}
		} else {
			cfg_log("renderer.%s ignored: unknown key", name);
		}
		return 1;
	}

	if (str_ieq(section, "socket")) {
		nx_socket_config_t *s = &cfg->socket;
		uint32_t u;
		if (str_ieq(name, "tcp_tx_buf_size")) {
			if (parse_u32(value, &u)) { s->tcp_tx_buf_size = u; s->has_tcp_tx_buf_size = true; }
			else cfg_log("socket.tcp_tx_buf_size=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "tcp_rx_buf_size")) {
			if (parse_u32(value, &u)) { s->tcp_rx_buf_size = u; s->has_tcp_rx_buf_size = true; }
			else cfg_log("socket.tcp_rx_buf_size=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "tcp_tx_buf_max_size")) {
			if (parse_u32(value, &u)) { s->tcp_tx_buf_max_size = u; s->has_tcp_tx_buf_max_size = true; }
			else cfg_log("socket.tcp_tx_buf_max_size=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "tcp_rx_buf_max_size")) {
			if (parse_u32(value, &u)) { s->tcp_rx_buf_max_size = u; s->has_tcp_rx_buf_max_size = true; }
			else cfg_log("socket.tcp_rx_buf_max_size=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "udp_tx_buf_size")) {
			if (parse_u32(value, &u)) { s->udp_tx_buf_size = u; s->has_udp_tx_buf_size = true; }
			else cfg_log("socket.udp_tx_buf_size=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "udp_rx_buf_size")) {
			if (parse_u32(value, &u)) { s->udp_rx_buf_size = u; s->has_udp_rx_buf_size = true; }
			else cfg_log("socket.udp_rx_buf_size=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "sb_efficiency")) {
			if (parse_u32(value, &u)) { s->sb_efficiency = u; s->has_sb_efficiency = true; }
			else cfg_log("socket.sb_efficiency=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "num_bsd_sessions")) {
			if (parse_u32(value, &u)) { s->num_bsd_sessions = u; s->has_num_bsd_sessions = true; }
			else cfg_log("socket.num_bsd_sessions=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "service_type")) {
			if (str_ieq(value, "auto")) { s->service_type = BsdServiceType_Auto; s->has_service_type = true; }
			else if (str_ieq(value, "user")) { s->service_type = BsdServiceType_User; s->has_service_type = true; }
			else if (str_ieq(value, "system")) { s->service_type = BsdServiceType_System; s->has_service_type = true; }
			else cfg_log("socket.service_type=\"%s\" not honored: invalid (use auto|user|system)", value);
		} else {
			cfg_log("socket.%s ignored: unknown key", name);
		}
		return 1;
	}

	if (str_ieq(section, "threadpool")) {
		nx_threadpool_config_t *t = &cfg->threadpool;
		uint32_t u;
		if (str_ieq(name, "size")) {
			if (parse_u32(value, &u) && u >= 1) { t->size = u; t->has_size = true; }
			else cfg_log("threadpool.size=\"%s\" not honored: invalid (positive count)", value);
		} else if (str_ieq(name, "stack_size")) {
			if (parse_u32(value, &u) && u >= 1) { t->stack_size = u; t->has_stack_size = true; }
			else cfg_log("threadpool.stack_size=\"%s\" not honored: invalid size", value);
		} else {
			cfg_log("threadpool.%s ignored: unknown key", name);
		}
		return 1;
	}

	if (str_ieq(section, "console")) {
		nx_console_config_t *c = &cfg->console;
		double d;
		uint32_t u;
		if (str_ieq(name, "font_size")) {
			if (parse_double(value, &d) && d > 0) { c->font_size = d; c->has_font_size = true; }
			else cfg_log("console.font_size=\"%s\" not honored: invalid (positive number)", value);
		} else if (str_ieq(name, "line_height")) {
			if (parse_double(value, &d) && d > 0) { c->line_height = d; c->has_line_height = true; }
			else cfg_log("console.line_height=\"%s\" not honored: invalid (positive number)", value);
		} else if (str_ieq(name, "scrollback")) {
			if (parse_u32(value, &u)) { c->scrollback = u; c->has_scrollback = true; }
			else cfg_log("console.scrollback=\"%s\" not honored: invalid", value);
		} else if (str_ieq(name, "cursor_style")) {
			if (str_ieq(value, "block")) c->cursor_style = NX_CURSOR_BLOCK;
			else if (str_ieq(value, "underline")) c->cursor_style = NX_CURSOR_UNDERLINE;
			else if (str_ieq(value, "bar")) c->cursor_style = NX_CURSOR_BAR;
			else cfg_log("console.cursor_style=\"%s\" not honored: invalid (use block|underline|bar)", value);
		} else if (str_ieq(name, "cursor_opacity")) {
			if (parse_double(value, &d) && d <= 1.0) { c->cursor_opacity = d; c->has_cursor_opacity = true; }
			else cfg_log("console.cursor_opacity=\"%s\" not honored: invalid (0-1)", value);
		} else if (str_ieq(name, "background")) {
			set_color(&c->background, value);
		} else if (str_ieq(name, "foreground")) {
			set_color(&c->foreground, value);
		} else if (str_ieq(name, "cursor")) {
			set_color(&c->cursor, value);
		} else {
			// Try the ANSI palette keys (black..bright_white).
			bool matched = false;
			for (int i = 0; i < 16; i++) {
				if (str_ieq(name, CONSOLE_ANSI_KEYS[i])) {
					set_color(&c->ansi[i], value);
					matched = true;
					break;
				}
			}
			if (!matched)
				cfg_log("console.%s ignored: unknown key", name);
		}
		return 1;
	}

	// [runtime] is consumed by the slim bootstrap LAUNCHER (it selects which
	// shared runtime NRO to chainload), not by the runtime itself. The same
	// nxjs.ini ships inside a slim app's RomFS and is read here too, so accept
	// and ignore the section silently rather than logging it as unknown.
	if (str_ieq(section, "runtime")) {
		return 1;
	}

	cfg_log("[%s] section ignored: unknown", section);
	return 1;
}

// ---------------------------------------------------------------------------
// Public API.
// ---------------------------------------------------------------------------

void nx_config_defaults(nx_config_t *cfg) {
	memset(cfg, 0, sizeof(*cfg));
	cfg->jit = NX_JIT_AUTO;
	cfg->renderer = NX_RENDER_AUTO;
	cfg->v8_flags = NULL;
	cfg->heap_limit = 0;
	cfg->code_headroom_mb = NX_CODE_HEADROOM_AUTO;
	cfg->gpu_cache_mib = NX_GPU_CACHE_AUTO;
	cfg->loaded = false;
}

bool nx_config_load(nx_config_t *cfg, const char *ini_path) {
	if (!ini_path)
		return false;
	// fopen-based: missing file -> ini_parse returns -1. A positive return is
	// the line number of the first parse error (handler still ran for valid
	// lines). We treat any "file existed" outcome as loaded.
	int rc = ini_parse(ini_path, ini_cb, cfg);
	if (rc == -1) {
		// File not present (or unreadable). Silent for the common no-config
		// case; nothing to honor/explain.
		return false;
	}
	if (rc == -2) {
		cfg_log("nxjs.ini at %s: out of memory while parsing; using defaults",
		        ini_path);
		return false;
	}
	if (rc > 0) {
		cfg_log("nxjs.ini at %s: parse error near line %d (other settings "
		        "still applied)",
		        ini_path, rc);
	}
	cfg->loaded = true;
	return true;
}

char *nx_config_ini_path_for(const char *entrypoint) {
	if (!entrypoint)
		return NULL;
	// Replace the basename (everything after the last '/') with "nxjs.ini".
	const char *slash = strrchr(entrypoint, '/');
	size_t dir_len = slash ? (size_t)(slash - entrypoint) + 1 : 0;
	const char *fname = "nxjs.ini";
	size_t out_len = dir_len + strlen(fname) + 1;
	char *out = (char *)malloc(out_len);
	if (!out)
		return NULL;
	if (dir_len)
		memcpy(out, entrypoint, dir_len);
	memcpy(out + dir_len, fname, strlen(fname) + 1);
	return out;
}

void nx_config_apply_socket(SocketInitConfig *base, const nx_config_t *cfg,
                            bool tight_memory) {
	const nx_socket_config_t *s = &cfg->socket;

	if (s->has_tcp_tx_buf_size)
		base->tcp_tx_buf_size = s->tcp_tx_buf_size;
	if (s->has_tcp_rx_buf_size)
		base->tcp_rx_buf_size = s->tcp_rx_buf_size;
	if (s->has_tcp_tx_buf_max_size)
		base->tcp_tx_buf_max_size = s->tcp_tx_buf_max_size;
	if (s->has_tcp_rx_buf_max_size)
		base->tcp_rx_buf_max_size = s->tcp_rx_buf_max_size;
	if (s->has_udp_tx_buf_size)
		base->udp_tx_buf_size = s->udp_tx_buf_size;
	if (s->has_udp_rx_buf_size)
		base->udp_rx_buf_size = s->udp_rx_buf_size;
	if (s->has_sb_efficiency)
		base->sb_efficiency = s->sb_efficiency;
	if (s->has_num_bsd_sessions)
		base->num_bsd_sessions = s->num_bsd_sessions;
	if (s->has_service_type)
		base->bsd_service_type = s->service_type;

	// --- Validation / clamping (each clamp is logged) -----------------------

	// A buffer's initial size can't exceed its max.
	if (base->tcp_tx_buf_size > base->tcp_tx_buf_max_size) {
		cfg_log("socket.tcp_tx_buf_size (%u) not honored: exceeds "
		        "tcp_tx_buf_max_size (%u), clamped",
		        base->tcp_tx_buf_size, base->tcp_tx_buf_max_size);
		base->tcp_tx_buf_size = base->tcp_tx_buf_max_size;
	}
	if (base->tcp_rx_buf_size > base->tcp_rx_buf_max_size) {
		cfg_log("socket.tcp_rx_buf_size (%u) not honored: exceeds "
		        "tcp_rx_buf_max_size (%u), clamped",
		        base->tcp_rx_buf_size, base->tcp_rx_buf_max_size);
		base->tcp_rx_buf_size = base->tcp_rx_buf_max_size;
	}

	// sb_efficiency: libnx expects a small multiplier; clamp to [1, 8].
	if (base->sb_efficiency < 1) {
		cfg_log("socket.sb_efficiency (%u) not honored: below 1, clamped to 1",
		        base->sb_efficiency);
		base->sb_efficiency = 1;
	} else if (base->sb_efficiency > 8) {
		cfg_log("socket.sb_efficiency (%u) not honored: above 8, clamped to 8",
		        base->sb_efficiency);
		base->sb_efficiency = 8;
	}

	// num_bsd_sessions: valid range is 1..4 (libnx allows up to 4 sockets
	// service sessions). Clamp.
	if (base->num_bsd_sessions < 1) {
		cfg_log("socket.num_bsd_sessions (%u) not honored: below 1, clamped to 1",
		        base->num_bsd_sessions);
		base->num_bsd_sessions = 1;
	} else if (base->num_bsd_sessions > 4) {
		cfg_log("socket.num_bsd_sessions (%u) not honored: above 4, clamped to 4",
		        base->num_bsd_sessions);
		base->num_bsd_sessions = 4;
	}

	// Total transfer-memory reservation must fit the regime. The libnx BSD
	// service reserves ~(tcp_tx_max + tcp_rx_max + udp_tx + udp_rx) *
	// sb_efficiency. Cap the reservation: 16 MiB in applet (tight) mode,
	// 64 MiB in application mode. Because socketInitialize() failure is fatal
	// (diagAbortWithResult), this clamp MUST be exhaustive: a misconfigured INI
	// with huge UDP buffers and/or a high sb_efficiency must not be able to keep
	// the reservation above the cap. Shrink contributors in order — TCP max
	// buffers, then UDP buffers, then sb_efficiency — and as a final guarantee
	// solve for an sb_efficiency that fits.
	uint64_t cap = (tight_memory ? 16ull : 64ull) * 1024 * 1024;
	auto reservation = [&]() -> uint64_t {
		uint64_t per = (uint64_t)base->tcp_tx_buf_max_size +
		               base->tcp_rx_buf_max_size + base->udp_tx_buf_size +
		               base->udp_rx_buf_size;
		return per * base->sb_efficiency;
	};
	if (reservation() > cap) {
		cfg_log("socket buffers not honored: reservation ~%llu MiB exceeds the "
		        "%llu MiB %s-mode cap; shrinking buffers to fit",
		        (unsigned long long)(reservation() / (1024 * 1024)),
		        (unsigned long long)(cap / (1024 * 1024)),
		        tight_memory ? "applet" : "application");

		// 1) Halve TCP max buffers down to a 16 KiB floor.
		while (reservation() > cap &&
		       (base->tcp_tx_buf_max_size > 16384 ||
		        base->tcp_rx_buf_max_size > 16384)) {
			if (base->tcp_tx_buf_max_size > 16384)
				base->tcp_tx_buf_max_size /= 2;
			if (base->tcp_rx_buf_max_size > 16384)
				base->tcp_rx_buf_max_size /= 2;
		}
		// 2) Halve UDP buffers down to a 2 KiB floor.
		while (reservation() > cap && (base->udp_tx_buf_size > 2048 ||
		                               base->udp_rx_buf_size > 2048)) {
			if (base->udp_tx_buf_size > 2048)
				base->udp_tx_buf_size /= 2;
			if (base->udp_rx_buf_size > 2048)
				base->udp_rx_buf_size /= 2;
		}
		// 3) Solve for the largest sb_efficiency (>=1) that still fits, as a
		//    hard guarantee the reservation is <= cap.
		uint64_t per = (uint64_t)base->tcp_tx_buf_max_size +
		               base->tcp_rx_buf_max_size + base->udp_tx_buf_size +
		               base->udp_rx_buf_size;
		if (per == 0)
			per = 1; // avoid div-by-zero; reservation is then 0 anyway
		uint64_t max_eff = cap / per;
		if (max_eff < 1)
			max_eff = 1; // floor: a single set of buffers must still be allowed
		if (base->sb_efficiency > max_eff)
			base->sb_efficiency = (uint32_t)max_eff;

		// Keep the initial buffer sizes <= their (possibly reduced) max.
		if (base->tcp_tx_buf_size > base->tcp_tx_buf_max_size)
			base->tcp_tx_buf_size = base->tcp_tx_buf_max_size;
		if (base->tcp_rx_buf_size > base->tcp_rx_buf_max_size)
			base->tcp_rx_buf_size = base->tcp_rx_buf_max_size;

		cfg_log("socket reservation now ~%llu MiB (cap %llu MiB)",
		        (unsigned long long)(reservation() / (1024 * 1024)),
		        (unsigned long long)(cap / (1024 * 1024)));
	}
}

void nx_config_apply_threadpool(nx_config_t *cfg, bool tight_memory) {
	// Defaults: 4 workers (libuv's own default count) x 1 MiB stacks; applet
	// (tight) regime drops to 2 workers — its native heap is ~168 MiB total
	// with >130 MiB consumed at startup, so both the stacks themselves and
	// the peak of concurrent native work buffers matter. The worker callbacks
	// are shallow C (zlib's 16 KiB stack CHUNK buffers, mbedtls handshakes
	// ~64 KiB, image decoders), so 1 MiB has ample margin while costing
	// 2-4 MiB total instead of upstream libuv's 32 MiB — which applet mode
	// could not commit next to the JIT code arena, turning the first async op
	// into a hard libuv abort().
	uint32_t size = tight_memory ? 2 : 4;
	uint32_t stack_size = 1024 * 1024;

	if (cfg->threadpool.has_size) {
		uint32_t req = cfg->threadpool.size;
		size = req;
		// libuv caps at 1024; anything past 64 is senseless on 4 cores and
		// multiplies stack cost.
		if (size > 64) {
			cfg_log("threadpool.size=%u not honored: above 64, clamped", req);
			size = 64;
		}
	}
	if (cfg->threadpool.has_stack_size) {
		uint32_t req = cfg->threadpool.stack_size;
		stack_size = req;
		// Floor: a too-small worker stack overflows silently (memory
		// corruption, not a clean error). 256 KiB is the smallest safe value
		// given the native work callbacks. Cap at 32 MiB (upstream is 8 MiB).
		if (stack_size < 256 * 1024) {
			cfg_log("threadpool.stack_size=%u not honored: below 256 KiB "
			        "floor, clamped",
			        req);
			stack_size = 256 * 1024;
		} else if (stack_size > 32 * 1024 * 1024) {
			cfg_log("threadpool.stack_size=%u not honored: above 32 MiB cap, "
			        "clamped",
			        req);
			stack_size = 32 * 1024 * 1024;
		}
	}

	cfg->effective_threadpool_size = size;
	cfg->effective_threadpool_stack_size = stack_size;
}

void nx_config_free(nx_config_t *cfg) {
	if (!cfg)
		return;
	free(cfg->v8_flags);
	cfg->v8_flags = NULL;
	nx_console_config_t *c = &cfg->console;
	free(c->background);
	free(c->foreground);
	free(c->cursor);
	c->background = c->foreground = c->cursor = NULL;
	for (int i = 0; i < 16; i++) {
		free(c->ansi[i]);
		c->ansi[i] = NULL;
	}
}
