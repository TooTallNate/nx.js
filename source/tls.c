#include "tls.h"
#include "error.h"
#include "poll.h"
#include <arpa/inet.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>

static JSClassID nx_tls_context_class_id;

static nx_tls_context_t *nx_tls_context_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_tls_context_class_id);
}

static void finalizer_tls_context(JSRuntime *rt, JSValue val) {
	fprintf(stderr, "finalizer_tls_context\n");
	nx_tls_context_t *data = JS_GetOpaque(val, nx_tls_context_class_id);
	if (data) {
		mbedtls_net_free(&data->server_fd);
		mbedtls_ssl_free(&data->ssl);
		mbedtls_ssl_config_free(&data->conf);
		js_free_rt(rt, data);
	}
}

void nx_tls_on_connect(nx_poll_t *p, nx_tls_connect_t *req) {
	nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
	JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

	if (req->err) {
		/* Error during TLS handshake */
		char error_buf[100];
		mbedtls_strerror(req->err, error_buf, 100);
		args[0] = JS_NewError(req_cb->context);
		JS_SetPropertyStr(req_cb->context, args[0], "message",
						  JS_NewString(req_cb->context, error_buf));
	} else {
		/* Handshake complete */
		args[1] = req_cb->buffer;
	}

	JSValue ret_val =
		JS_Call(req_cb->context, req_cb->callback, JS_NULL, 2, args);
	JS_FreeValue(req_cb->context, req_cb->buffer);
	JS_FreeValue(req_cb->context, req_cb->callback);
	if (JS_IsException(ret_val)) {
		nx_emit_error_event(req_cb->context);
	}
	JS_FreeValue(req_cb->context, ret_val);
	free(req_cb);
	free(req);
}

void nx_tls_do_handshake(nx_poll_t *p, nx_watcher_t *watcher, int revents) {
	nx_tls_connect_t *req = (nx_tls_connect_t *)watcher;
	int err = mbedtls_ssl_handshake(&req->data->ssl);
	if (err == MBEDTLS_ERR_SSL_WANT_READ || err == MBEDTLS_ERR_SSL_WANT_WRITE) {
		// Handshake not completed, wait for more events
		return;
	}
	nx_remove_watcher(p, watcher);
	req->err = err;
	req->callback(p, req);
}

JSValue nx_tls_handshake(JSContext *ctx, JSValueConst this_val, int argc,
						 JSValueConst *argv) {
	int ret;
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	if (!nx_ctx->mbedtls_initialized) {
		mbedtls_entropy_init(&nx_ctx->entropy);
		mbedtls_ctr_drbg_init(&nx_ctx->ctr_drbg);

		const char *pers = "client";

		// Seed the RNG
		if ((ret = mbedtls_ctr_drbg_seed(
				 &nx_ctx->ctr_drbg, mbedtls_entropy_func, &nx_ctx->entropy,
				 (const unsigned char *)pers, strlen(pers))) != 0) {
			char error_buf[100];
			mbedtls_strerror(ret, error_buf, 100);
			JS_ThrowTypeError(ctx, "Failed seeding RNG: %s", error_buf);
			return JS_EXCEPTION;
		}

		nx_ctx->mbedtls_initialized = true;
	}

	int fd;
	const char *hostname = JS_ToCString(ctx, argv[2]);
	if (!hostname || JS_ToInt32(ctx, &fd, argv[1])) {
		JS_ThrowTypeError(ctx, "invalid input");
		return JS_EXCEPTION;
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_tls_context_class_id);
	nx_tls_context_t *data = js_mallocz(ctx, sizeof(nx_tls_context_t));
	if (!data) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, data);

	data->server_fd.fd = fd;
	mbedtls_ssl_init(&data->ssl);
	mbedtls_ssl_config_init(&data->conf);

	// Setup the SSL/TLS structure and set the hostname for Server Name
	// Indication (SNI)
	if ((ret = mbedtls_ssl_config_defaults(&data->conf, MBEDTLS_SSL_IS_CLIENT,
										   MBEDTLS_SSL_TRANSPORT_STREAM,
										   MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		JS_ThrowTypeError(ctx, "Failed setting SSL config defaults: %s",
						  error_buf);
		return JS_EXCEPTION;
	}
	mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(&data->conf, mbedtls_ctr_drbg_random,
						 &nx_ctx->ctr_drbg);
	if ((ret = mbedtls_ssl_set_hostname(&data->ssl, hostname)) != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		JS_ThrowTypeError(ctx, "Failed setting hostname: %s", error_buf);
		return JS_EXCEPTION;
	}
	mbedtls_ssl_set_bio(&data->ssl, &data->server_fd, mbedtls_net_send,
						mbedtls_net_recv, NULL);
	if ((ret = mbedtls_ssl_setup(&data->ssl, &data->conf)) != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		JS_ThrowTypeError(ctx, "Failed setting up SSL: %s", error_buf);
		return JS_EXCEPTION;
	}

	JS_FreeCString(ctx, hostname);

	nx_tls_connect_t *req = malloc(sizeof(nx_tls_connect_t));
	nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
	req_cb->context = ctx;
	req_cb->callback = JS_DupValue(ctx, argv[0]);
	req_cb->buffer = JS_DupValue(ctx, obj);
	req->opaque = req_cb;
	req->data = data;
	req->watcher_callback = nx_tls_do_handshake;
	req->callback = nx_tls_on_connect;
	req->fd = fd;
	req->events = POLLIN | POLLOUT | POLLERR;

	nx_add_watcher(&nx_ctx->poll, (nx_watcher_t *)req);
	nx_tls_do_handshake(&nx_ctx->poll, (nx_watcher_t *)req, 0);

	return JS_UNDEFINED;
}

void nx_tls_do_read(nx_poll_t *p, nx_watcher_t *watcher, int revents) {
	nx_tls_read_t *req = (nx_tls_read_t *)watcher;
	nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;

	size_t total_read = 0;
	int ret;

	while (total_read < req->buffer_size) {
		ret = mbedtls_ssl_read(&req->data->ssl, req->buffer + total_read,
							   req->buffer_size - total_read);

		if (ret > 0) {
			total_read += ret;
		} else if (ret == 0) {
			// End of data stream reached
			break;
		} else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
			// Need more data, break the loop if we've read anything
			if (total_read > 0) {
				break;
			}
			return;
		} else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			// Connection closed
			break;
		} else if (ret < 0) {
			// Error occurred
			req->err = ret;
			break;
		}
	}

	nx_remove_watcher(p, watcher);

	JSContext *ctx = req_cb->context;
	JS_FreeValue(ctx, req_cb->buffer);

	JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

	if (req->err) {
		/* Error during read. */
		char error_buf[100];
		mbedtls_strerror(req->err, error_buf, 100);
		args[0] = JS_NewError(ctx);
		JS_SetPropertyStr(ctx, args[0], "message",
						  JS_NewString(ctx, error_buf));
	} else {
		args[1] = JS_NewInt32(ctx, total_read);
	}

	JSValue ret_val = JS_Call(ctx, req_cb->callback, JS_NULL, 2, args);
	JS_FreeValue(ctx, args[0]);
	JS_FreeValue(ctx, args[1]);
	JS_FreeValue(ctx, req_cb->callback);
	if (JS_IsException(ret_val)) {
		nx_emit_error_event(ctx);
	}
	JS_FreeValue(ctx, ret_val);
	free(req_cb);
	free(req);
}

JSValue nx_tls_read(JSContext *ctx, JSValueConst this_val, int argc,
					JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	size_t buffer_size;

	nx_tls_context_t *data = nx_tls_context_get(ctx, argv[1]);
	if (!data)
		return JS_EXCEPTION;

	uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, argv[2]);
	if (!buffer)
		return JS_EXCEPTION;

	JSValue buffer_value = JS_DupValue(ctx, argv[2]);

	nx_tls_read_t *req = malloc(sizeof(nx_tls_read_t));
	nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
	req_cb->context = ctx;
	req_cb->callback = JS_DupValue(ctx, argv[0]);
	req_cb->buffer = buffer_value;
	req->fd = data->server_fd.fd;
	req->events = POLLIN | POLLERR;
	req->err = 0;
	req->watcher_callback = nx_tls_do_read;
	req->opaque = req_cb;
	req->data = data;
	req->buffer = buffer;
	req->buffer_size = buffer_size;

	nx_add_watcher(&nx_ctx->poll, (nx_watcher_t *)req);
	nx_tls_do_read(&nx_ctx->poll, (nx_watcher_t *)req, 0);

	return JS_UNDEFINED;
}

void nx_tls_do_write(nx_poll_t *p, nx_watcher_t *watcher, int revents) {
	nx_tls_write_t *req = (nx_tls_write_t *)watcher;
	nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;

	int ret = mbedtls_ssl_write(&req->data->ssl, req->buffer, req->buffer_size);

	if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
		return;
	}

	JSContext *ctx = req_cb->context;
	JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

	if (ret < 0) {
		/* Error during write */
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		args[0] = JS_NewError(ctx);
		JS_SetPropertyStr(ctx, args[0], "message",
						  JS_NewString(ctx, error_buf));
	} else {
		req->bytes_written += ret;
		if (req->bytes_written < req->buffer_size) {
			// Not all data was written, need to wait before trying again
			return;
		}

		args[1] = JS_NewInt32(ctx, ret);
	}

	// If we got to here then all the data was written
	nx_remove_watcher(p, watcher);

	JS_FreeValue(ctx, req_cb->buffer);

	JSValue ret_val = JS_Call(ctx, req_cb->callback, JS_NULL, 2, args);
	JS_FreeValue(ctx, args[0]);
	JS_FreeValue(ctx, args[1]);
	JS_FreeValue(ctx, req_cb->callback);
	if (JS_IsException(ret_val)) {
		nx_emit_error_event(ctx);
	}
	JS_FreeValue(ctx, ret_val);
	free(req_cb);
	free(req);
}

JSValue nx_tls_write(JSContext *ctx, JSValueConst this_val, int argc,
					 JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	size_t buffer_size;

	nx_tls_context_t *data = nx_tls_context_get(ctx, argv[1]);
	if (!data)
		return JS_EXCEPTION;

	uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, argv[2]);
	if (!buffer)
		return JS_EXCEPTION;

	JSValue buffer_value = JS_DupValue(ctx, argv[2]);

	nx_tls_write_t *req = malloc(sizeof(nx_tls_write_t));
	nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
	req_cb->context = ctx;
	req_cb->callback = JS_DupValue(ctx, argv[0]);
	req_cb->buffer = buffer_value;
	req->fd = data->server_fd.fd;
	req->events = POLLOUT | POLLERR;
	req->err = 0;
	req->watcher_callback = nx_tls_do_write;
	req->opaque = req_cb;
	req->data = data;
	req->buffer = buffer;
	req->buffer_size = buffer_size;
	req->bytes_written = 0;

	nx_add_watcher(&nx_ctx->poll, (nx_watcher_t *)req);
	nx_tls_do_write(&nx_ctx->poll, (nx_watcher_t *)req, 0);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("tlsHandshake", 0, nx_tls_handshake),
	JS_CFUNC_DEF("tlsRead", 0, nx_tls_read),
	JS_CFUNC_DEF("tlsWrite", 0, nx_tls_write),
};

void nx_init_tls(JSContext *ctx, JSValueConst init_obj) {

	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_tls_context_class_id);
	JSClassDef nx_tls_context_class = {
		"TlsContext",
		.finalizer = finalizer_tls_context,
	};
	JS_NewClass(rt, nx_tls_context_class_id, &nx_tls_context_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
