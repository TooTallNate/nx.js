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

typedef struct {
	mbedtls_net_context server_fd;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
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
	nx_ctx->ca_certs_loaded = true;
	return 0;
}

void free_tls_context(nx_tls_context_t *data) {
	mbedtls_net_free(&data->server_fd);
	mbedtls_ssl_free(&data->ssl);
	mbedtls_ssl_config_free(&data->conf);
	free(data);
}

Local<Value> mbedtls_error(Isolate *iso, int err) {
	char buf[128];
	mbedtls_strerror(err, buf, sizeof(buf));
	return Exception::Error(nx_str(iso, buf));
}

// ---- a uv_poll-driven mbedtls operation ----
enum op_kind { OP_HANDSHAKE, OP_READ, OP_WRITE };

struct tls_op_t {
	uv_poll_t poll;
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

void tls_op_finish(tls_op_t *op, Local<Value> err, Local<Value> value) {
	Isolate *iso = op->iso;
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = op->callback.Get(iso);
	Local<Value> args[] = {err, value};
	uv_poll_stop(&op->poll);
	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
	op->callback.Reset();
	op->ctx_obj.Reset();
	op->buffer.Reset();
	uv_close((uv_handle_t *)&op->poll,
	         [](uv_handle_t *h) { delete static_cast<tls_op_t *>(h->data); });
}

void tls_poll_cb(uv_poll_t *handle, int status, int events) {
	tls_op_t *op = static_cast<tls_op_t *>(handle->data);
	Isolate *iso = op->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());

	if (status < 0) {
		tls_op_finish(op, Exception::Error(nx_str(iso, uv_strerror(status))),
		              Undefined(iso));
		return;
	}

	if (op->kind == OP_HANDSHAKE) {
		int err = mbedtls_ssl_handshake(&op->data->ssl);
		if (err == MBEDTLS_ERR_SSL_WANT_READ ||
		    err == MBEDTLS_ERR_SSL_WANT_WRITE)
			return; // keep polling
		if (err != 0) {
			tls_op_finish(op, mbedtls_error(iso, err), Undefined(iso));
		} else {
			tls_op_finish(op, Undefined(iso), op->ctx_obj.Get(iso));
		}
		return;
	}

	if (op->kind == OP_READ) {
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
			} else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
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
		return;
	}

	// OP_WRITE
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

tls_op_t *tls_op_new(Isolate *iso, op_kind kind, nx_tls_context_t *data,
                     int fd, Local<Function> cb) {
	tls_op_t *op = new tls_op_t();
	op->iso = iso;
	op->kind = kind;
	op->data = data;
	op->callback.Reset(iso, cb);
	op->done = 0;
	uv_poll_init_socket(nx_ctx(iso)->loop, &op->poll, fd);
	op->poll.data = op;
	int evts =
	    (kind == OP_WRITE) ? UV_WRITABLE : (UV_READABLE | UV_WRITABLE);
	uv_poll_start(&op->poll, evts, tls_poll_cb);
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
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
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
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
}

} // namespace

void nx_init_tls(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "tlsHandshake", nx_tls_handshake);
	NX_SET_FUNC(init_obj, "tlsRead", nx_tls_read);
	NX_SET_FUNC(init_obj, "tlsWrite", nx_tls_write);
}
