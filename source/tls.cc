// TLS over libuv: mbedtls layered on a raw fd, driven by a uv_poll_t that
// retries the mbedtls operation on each readiness event until it completes
// (WANT_READ/WANT_WRITE -> keep polling). Same fd model as tcp.cc/udp.cc.
#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <stdio.h>
#include <string.h>
#include <switch/services/ssl.h>

using namespace v8;

namespace {

struct tls_op_t; // forward

typedef struct {
	mbedtls_net_context server_fd;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	// A TLS connection is exactly one fd. libuv allows only ONE uv_poll_t per
	// fd, and ops (handshake/read/write) are sequential, so we keep a SINGLE
	// persistent poll handle on the connection and reuse it across ops rather
	// than creating one per op (which caused uv_poll_init_socket EEXIST ->
	// uv_poll_start null-deref when the next op started before the previous
	// handle finished its async uv_close).
	uv_poll_t poll;
	bool poll_init;    // poll handle has been uv_poll_init_socket'd
	bool fd_closed;    // the underlying fd has been closed
	bool torn_down;    // connection resources released (idempotent guard)
	bool poll_closing; // uv_close(&poll) issued, awaiting its callback
	// A read and a write can be in flight CONCURRENTLY on the same connection
	// (fetch writes the request while the response readable stream is already
	// pulling). The single shared poll must therefore track BOTH; the poll
	// callback dispatches to each based on the fired events. The handshake uses
	// read_op (only one op exists during the handshake).
	tls_op_t *read_op;  // pending OP_HANDSHAKE or OP_READ (or null)
	tls_op_t *write_op; // pending OP_WRITE (or null)
} nx_tls_context_t;

// Built-in CA cert IDs to load individually (one-at-a-time works around a
// libnx SslCaCertificateId_All bounds-check bug). Trimmed comment list.
const u32 nx_ca_cert_ids[] = {
    1,    2,    3,    1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008,
    1009, 1010, 1011, 1012, 1013, 1014, 1015, 1016, 1017, 1018, 1019, 1020,
    1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031, 1032,
    1033, 1034, 1035, 1036, 1037, 1038, 1039, 1040, 1041, 1042, 1043, 1044,
    1045, 1046, 1047, 1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056,
    1057, 1058, 1059,
};

int nx_tls_load_ca_certs(nx_context_t *nx_ctx) {
	if (nx_ctx->ca_certs_loaded)
		return 0;
	mbedtls_x509_crt_init(&nx_ctx->ca_chain);
	Result rc = sslInitialize(1);
	if (R_FAILED(rc)) {
		fprintf(stderr, "sslInitialize() failed: 0x%x\n", (unsigned)rc);
		return -1;
	}
	int loaded = 0;
	int total = (int)(sizeof(nx_ca_cert_ids) / sizeof(nx_ca_cert_ids[0]));
	for (int i = 0; i < total; i++) {
		u32 id = nx_ca_cert_ids[i];
		u32 buf_size = 0;
		if (R_FAILED(sslGetCertificateBufSize(&id, 1, &buf_size)))
			continue;
		void *cert_buffer = malloc(buf_size);
		if (!cert_buffer)
			continue;
		u32 out_count = 0;
		if (R_FAILED(
		        sslGetCertificates(cert_buffer, buf_size, &id, 1, &out_count))) {
			free(cert_buffer);
			continue;
		}
		SslBuiltInCertificateInfo *info =
		    (SslBuiltInCertificateInfo *)cert_buffer;
		for (u32 j = 0; j < out_count; j++) {
			if (info[j].status != SslTrustedCertStatus_EnabledTrusted)
				continue;
			if (info[j].cert_data && info[j].cert_size > 0) {
				if (mbedtls_x509_crt_parse_der(&nx_ctx->ca_chain,
				                               (const unsigned char *)
				                                   info[j].cert_data,
				                               info[j].cert_size) == 0) {
					loaded++;
				}
			}
		}
		free(cert_buffer);
	}
	sslExit();
	fprintf(stderr, "Loaded %d system CA certificates\n", loaded);
	nx_ctx->ca_cert_count = loaded;
	nx_ctx->ca_certs_loaded = true;
	return 0;
}

// Eagerly release a TLS connection's RUNTIME resources (poll handle, fd,
// mbedtls state) without freeing the struct. Idempotent. The fd is closed
// EXACTLY ONCE and only AFTER its uv_poll_t has been uv_close()d — closing an
// fd while a uv_poll_t still references it corrupts the bsdsocket sysmodule
// (User Break). A TLS connection owns its fd; the TS layer must NOT also
// $.close it.
//
// The struct itself is freed ONLY by the GC finalizer (free_tls_context),
// never here — so there is no double-free with the wrap WeakRef that retains
// the pointer. By GC time any async uv_close issued here has long completed.
static void tls_teardown(nx_tls_context_t *data) {
	if (data->torn_down)
		return;
	data->torn_down = true;
	mbedtls_ssl_free(&data->ssl);
	mbedtls_ssl_config_free(&data->conf);
	if (data->poll_init) {
		data->poll_init = false;
		uv_poll_stop(&data->poll);
		uv_close((uv_handle_t *)&data->poll, [](uv_handle_t *h) {
			nx_tls_context_t *d = static_cast<nx_tls_context_t *>(h->data);
			if (!d->fd_closed && d->server_fd.fd >= 0) {
				close(d->server_fd.fd);
				d->fd_closed = true;
			}
			d->server_fd.fd = -1; // mbedtls_net_free() must not touch the fd
		});
		return;
	}
	if (!data->fd_closed && data->server_fd.fd >= 0) {
		close(data->server_fd.fd);
		data->fd_closed = true;
	}
	data->server_fd.fd = -1;
}

// GC finalizer: ensure resources are released, then free the struct. (If the
// poll was already uv_close()d via an explicit $.tlsClose, fd_closed is set and
// mbedtls_net_free below is a no-op on fd -1.)
void free_tls_context(nx_tls_context_t *data) {
	tls_teardown(data);
	mbedtls_net_free(&data->server_fd);
	free(data);
}

void tls_op_finish(tls_op_t *op, Local<Value> err, Local<Value> value);

// $.tlsClose(ctx): deterministic eager teardown of a TLS connection's runtime
// resources. The struct is reclaimed later by the GC finalizer.
void nx_tls_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_tls_context_t *data = nx::Unwrap<nx_tls_context_t>(info[0]);
	if (!data)
		return;
	// Settle any in-flight ops (read and/or write) so their awaiting JS
	// promises don't hang (e.g. the body stream was cancelled while a tlsRead
	// is pending on a keep-alive connection). Resolve with 0 (EOF / 0 bytes).
	if (data->write_op) {
		tls_op_t *op = data->write_op;
		data->write_op = nullptr;
		tls_op_finish(op, Undefined(iso), Integer::NewFromUnsigned(iso, 0));
	}
	if (data->read_op) {
		tls_op_t *op = data->read_op;
		data->read_op = nullptr;
		tls_op_finish(op, Undefined(iso), Integer::NewFromUnsigned(iso, 0));
	}
	tls_teardown(data);
}

Local<Value> mbedtls_error(Isolate *iso, int err) {
	char buf[128];
	mbedtls_strerror(err, buf, sizeof(buf));
	return Exception::Error(nx_str(iso, buf));
}

// ---- a uv_poll-driven mbedtls operation ----
enum op_kind { OP_HANDSHAKE, OP_READ, OP_WRITE };

struct tls_op_t {
	Isolate *iso;
	op_kind kind;
	nx_tls_context_t *data;
	Global<Function> callback;
	Global<Value> ctx_obj; // keep the TlsContext alive
	Global<Value> buffer;  // keep the ArrayBuffer alive (read/write)
	uint8_t *buf;
	size_t buf_size;
	size_t done; // bytes read/written so far
};

void tls_poll_refresh(nx_tls_context_t *data); // defined below

void tls_op_finish(tls_op_t *op, Local<Value> err, Local<Value> value) {
	Isolate *iso = op->iso;
	Local<Context> context = iso->GetCurrentContext();
	nx_tls_context_t *data = op->data;

	// Detach THIS op (read or write) before the callback (which may start a new
	// op on the same connection), then recompute the shared poll's interest —
	// the OTHER direction may still be pending and must keep polling.
	if (data->write_op == op)
		data->write_op = nullptr;
	if (data->read_op == op)
		data->read_op = nullptr;
	tls_poll_refresh(data);

	Local<Function> cb = op->callback.Get(iso);
	Local<Value> args[] = {err, value};
	op->callback.Reset();
	op->ctx_obj.Reset();
	op->buffer.Reset();
	delete op;

	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
}

// Drain decrypted data into op->buf. Finishes the op when it has bytes (or EOF
// / close-notify); returns without finishing (keep polling) on WANT_READ with
// nothing yet. Safe to call both from the poll callback AND once immediately
// after a read is queued — mbedtls may already hold decrypted bytes in its
// internal buffer (read while completing the handshake / a prior record), in
// which case the socket fd never becomes readable again and the poll would
// otherwise never fire (the cause of the TLS read hang).
void tls_do_read(Isolate *iso, tls_op_t *op) {
	while (op->done < op->buf_size) {
		int ret = mbedtls_ssl_read(&op->data->ssl, op->buf + op->done,
		                           op->buf_size - op->done);
		if (ret > 0) {
			op->done += ret;
		} else if (ret == 0) {
			break;
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
		} else if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
			continue;
#endif
		} else if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
		           ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			if (op->done > 0)
				break;
			return; // keep polling
		} else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			break;
		} else {
			tls_op_finish(op, mbedtls_error(iso, ret), Undefined(iso));
			return;
		}
	}
	tls_op_finish(op, Undefined(iso),
	              Integer::NewFromUnsigned(iso, (uint32_t)op->done));
}

void tls_do_write(Isolate *iso, tls_op_t *op) {
	int ret = mbedtls_ssl_write(&op->data->ssl, op->buf + op->done,
	                            op->buf_size - op->done);
	if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
		return;
	if (ret < 0) {
		tls_op_finish(op, mbedtls_error(iso, ret), Undefined(iso));
		return;
	}
	op->done += ret;
	if (op->done < op->buf_size)
		return; // more to write; keep polling
	tls_op_finish(op, Undefined(iso),
	              Integer::NewFromUnsigned(iso, (uint32_t)op->done));
}

// Drive a single op (handshake/read/write) one step. Used by the poll callback
// and by the immediate-attempt paths.
void tls_drive_op(Isolate *iso, tls_op_t *op, int status) {
	if (status < 0) {
		tls_op_finish(op, Exception::Error(nx_str(iso, uv_strerror(status))),
		              Undefined(iso));
		return;
	}
	if (op->kind == OP_HANDSHAKE) {
		nx_tls_context_t *data = op->data;
		int err = mbedtls_ssl_handshake(&data->ssl);
		if (err == MBEDTLS_ERR_SSL_WANT_READ ||
		    err == MBEDTLS_ERR_SSL_WANT_WRITE)
			return; // keep polling
		if (err != 0) {
			if (err == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
				uint32_t flags = mbedtls_ssl_get_verify_result(&data->ssl);
				char info[512];
				mbedtls_x509_crt_verify_info(info, sizeof(info), "", flags);
				char peer[512] = "";
				const mbedtls_x509_crt *pc =
				    mbedtls_ssl_get_peer_cert(&data->ssl);
				if (pc) {
					char subj[256] = "", iss[256] = "";
					mbedtls_x509_dn_gets(subj, sizeof(subj), &pc->subject);
					mbedtls_x509_dn_gets(iss, sizeof(iss), &pc->issuer);
					snprintf(peer, sizeof(peer),
					         " | leaf subject=[%s] issuer=[%s]", subj, iss);
				}
				char msg[1200];
				snprintf(msg, sizeof(msg),
				         "X509 verification failed (flags=0x%08x): %s%s", flags,
				         info, peer);
				tls_op_finish(op, Exception::Error(nx_str(iso, msg)),
				              Undefined(iso));
			} else {
				tls_op_finish(op, mbedtls_error(iso, err), Undefined(iso));
			}
			// Handshake failed: JS never got a TlsContext handle, so it can't
			// $.tlsClose it — tear the connection down natively.
			tls_teardown(data);
		} else {
			tls_op_finish(op, Undefined(iso), op->ctx_obj.Get(iso));
		}
		return;
	}
	if (op->kind == OP_READ) {
		tls_do_read(iso, op);
		return;
	}
	tls_do_write(iso, op);
}

void tls_poll_cb(uv_poll_t *handle, int status, int events) {
	nx_tls_context_t *data = static_cast<nx_tls_context_t *>(handle->data);
	if (!data->read_op && !data->write_op)
		return;
	Isolate *iso =
	    data->read_op ? data->read_op->iso : data->write_op->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());

	// Drive the WRITE first (so a request is flushed before we try to read the
	// response), then the READ. Either may finish + detach itself; capture the
	// pointers up front since tls_op_finish nulls them. mbedtls is happy to be
	// driven on any readiness (it manages its own WANT_READ/WANT_WRITE).
	tls_op_t *wop = data->write_op;
	tls_op_t *rop = data->read_op;
	if (wop)
		tls_drive_op(iso, wop, status);
	// `data` may have been torn down if a handshake (read_op) failed; but a
	// handshake never coexists with a write_op, so if we drove a write the
	// read_op (if any) is a real read and `data` is still valid.
	if (rop && (data->read_op == rop))
		tls_drive_op(iso, rop, status);
}

// Recompute the shared poll's interest from the pending read/write ops and
// (re)start or stop it. Both read and write ask for R|W: mbedtls reads can
// require writability and writes can require readability (WANT_READ /
// WANT_WRITE, renegotiation), and we must drain on either readiness.
void tls_poll_refresh(nx_tls_context_t *data) {
	if (!data->poll_init || data->poll_closing)
		return;
	if (data->read_op || data->write_op) {
		uv_poll_start(&data->poll, UV_READABLE | UV_WRITABLE, tls_poll_cb);
	} else {
		uv_poll_stop(&data->poll);
	}
}

tls_op_t *tls_op_new(Isolate *iso, op_kind kind, nx_tls_context_t *data,
                     int fd, Local<Function> cb) {
	tls_op_t *op = new tls_op_t();
	op->iso = iso;
	op->kind = kind;
	op->data = data;
	op->callback.Reset(iso, cb);
	op->done = 0;

	// Lazily init the connection's single shared poll handle; reuse it for
	// every op. data->poll.data points at the CONTEXT; the op(s) are found via
	// data->read_op / data->write_op.
	if (!data->poll_init) {
		if (uv_poll_init_socket(nx_ctx(iso)->loop, &data->poll, fd) != 0) {
			nx_throw(iso, "failed to poll TLS socket");
			op->callback.Reset();
			delete op;
			return nullptr;
		}
		data->poll.data = data;
		data->poll_init = true;
	}
	if (kind == OP_WRITE)
		data->write_op = op;
	else
		data->read_op = op; // OP_HANDSHAKE or OP_READ
	tls_poll_refresh(data);
	return op;
}

void nx_tls_handshake(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_context_t *nx_ctx_ = nx_ctx(iso);

	if (!nx_ctx_->mbedtls_initialized) {
		mbedtls_entropy_init(&nx_ctx_->entropy);
		mbedtls_ctr_drbg_init(&nx_ctx_->ctr_drbg);
		const char *pers = "client";
		int ret = mbedtls_ctr_drbg_seed(
		    &nx_ctx_->ctr_drbg, mbedtls_entropy_func, &nx_ctx_->entropy,
		    (const unsigned char *)pers, strlen(pers));
		if (ret != 0) {
			iso->ThrowException(mbedtls_error(iso, ret));
			return;
		}
		nx_ctx_->mbedtls_initialized = true;
	}

	Local<Function> cb = info[0].As<Function>();
	int fd = 0;
	if (!info[1]->Int32Value(context).To(&fd)) {
		nx_throw(iso, "invalid input");
		return;
	}
	String::Utf8Value hostname(iso, info[2]);
	int reject_unauthorized = 1;
	if (info.Length() > 3 && info[3]->IsBoolean()) {
		reject_unauthorized = info[3]->BooleanValue(iso) ? 1 : 0;
	}

	Local<Object> obj = nx::NewWrapped(iso);
	nx_tls_context_t *data =
	    (nx_tls_context_t *)calloc(1, sizeof(nx_tls_context_t));
	nx::Wrap<nx_tls_context_t>(iso, obj, data, free_tls_context);
	data->server_fd.fd = fd;
	mbedtls_ssl_init(&data->ssl);
	mbedtls_ssl_config_init(&data->conf);

	int ret;
	if ((ret = mbedtls_ssl_config_defaults(&data->conf,
	                                        MBEDTLS_SSL_IS_CLIENT,
	                                        MBEDTLS_SSL_TRANSPORT_STREAM,
	                                        MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		iso->ThrowException(mbedtls_error(iso, ret));
		return;
	}
	if (reject_unauthorized && nx_tls_load_ca_certs(nx_ctx_) == 0) {
		mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
		mbedtls_ssl_conf_ca_chain(&data->conf, &nx_ctx_->ca_chain, NULL);
	} else {
		if (reject_unauthorized) {
			fprintf(stderr, "Warning: failed to load system CA certs, TLS "
			                "verification disabled\n");
		}
		mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_NONE);
	}
	mbedtls_ssl_conf_rng(&data->conf, mbedtls_ctr_drbg_random,
	                     &nx_ctx_->ctr_drbg);
	if ((ret = mbedtls_ssl_set_hostname(&data->ssl,
	                                    *hostname ? *hostname : "")) != 0) {
		iso->ThrowException(mbedtls_error(iso, ret));
		return;
	}
	mbedtls_ssl_set_bio(&data->ssl, &data->server_fd, mbedtls_net_send,
	                    mbedtls_net_recv, NULL);
	if ((ret = mbedtls_ssl_setup(&data->ssl, &data->conf)) != 0) {
		iso->ThrowException(mbedtls_error(iso, ret));
		return;
	}

	tls_op_t *op = tls_op_new(iso, OP_HANDSHAKE, data, fd, cb);
	if (!op)
		return;
	op->ctx_obj.Reset(iso, obj);
}

void nx_tls_read(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Function> cb = info[0].As<Function>();
	nx_tls_context_t *data = nx::Unwrap<nx_tls_context_t>(info[1]);
	if (!data)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	tls_op_t *op = tls_op_new(iso, OP_READ, data, data->server_fd.fd, cb);
	if (!op)
		return;
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
	// Attempt the read immediately: mbedtls may already hold decrypted bytes
	// in its internal buffer (read off the socket during the handshake or a
	// previous record), in which case the fd will NOT become readable and the
	// poll would never fire. tls_do_read finishes the op now if data is ready,
	// or returns to keep polling on WANT_READ.
	tls_do_read(iso, op);
}

void nx_tls_write(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Function> cb = info[0].As<Function>();
	nx_tls_context_t *data = nx::Unwrap<nx_tls_context_t>(info[1]);
	if (!data)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	tls_op_t *op = tls_op_new(iso, OP_WRITE, data, data->server_fd.fd, cb);
	if (!op)
		return;
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
}

} // namespace

void nx_init_tls(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "tlsHandshake", nx_tls_handshake);
	NX_SET_FUNC(init_obj, "tlsRead", nx_tls_read);
	NX_SET_FUNC(init_obj, "tlsWrite", nx_tls_write);
	NX_SET_FUNC(init_obj, "tlsClose", nx_tls_close);
}
