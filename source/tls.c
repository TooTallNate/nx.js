#include "tls.h"
#include "error.h"
#include "poll.h"
#include <arpa/inet.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <switch/services/ssl.h>

static JSClassID nx_tls_context_class_id;

static nx_tls_context_t *nx_tls_context_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_tls_context_class_id);
}

// Known built-in CA certificate IDs to load individually.
// Loading one at a time avoids a libnx bug where sslGetCertificates()
// with SslCaCertificateId_All can fail with LibnxError_ShouldNotHappen
// due to a bounds-check issue in the pointer-fixup loop.
static const u32 nx_ca_cert_ids[] = {
	1,    // NintendoCAG3
	2,    // NintendoClass2CAG3
	3,    // NintendoRootCAG4 [16.0.0+]
	1000, // AmazonRootCA1
	1001, // StarfieldServicesRootCertificateAuthorityG2
	1002, // AddTrustExternalCARoot
	1003, // COMODOCertificationAuthority
	1004, // UTNDATACorpSGC
	1005, // UTNUSERFirstHardware
	1006, // BaltimoreCyberTrustRoot
	1007, // CybertrustGlobalRoot
	1008, // VerizonGlobalRootCA
	1009, // DigiCertAssuredIDRootCA
	1010, // DigiCertAssuredIDRootG2
	1011, // DigiCertGlobalRootCA
	1012, // DigiCertGlobalRootG2
	1013, // DigiCertHighAssuranceEVRootCA
	1014, // EntrustnetCertificationAuthority2048
	1015, // EntrustRootCertificationAuthority
	1016, // EntrustRootCertificationAuthorityG2
	1017, // GeoTrustGlobalCA2
	1018, // GeoTrustGlobalCA
	1019, // GeoTrustPrimaryCertificationAuthorityG3
	1020, // GeoTrustPrimaryCertificationAuthority
	1021, // GlobalSignRootCA
	1022, // GlobalSignRootCAR2
	1023, // GlobalSignRootCAR3
	1024, // GoDaddyClass2CertificationAuthority
	1025, // GoDaddyRootCertificateAuthorityG2
	1026, // StarfieldClass2CertificationAuthority
	1027, // StarfieldRootCertificateAuthorityG2
	1028, // thawtePrimaryRootCAG3
	1029, // thawtePrimaryRootCA
	1030, // VeriSignClass3PublicPrimaryCertificationAuthorityG3
	1031, // VeriSignClass3PublicPrimaryCertificationAuthorityG5
	1032, // VeriSignUniversalRootCertificationAuthority
	1033, // DSTRootCAX3 [6.0.0+]
	1034, // USERTrustRsaCertificationAuthority [10.0.3+]
	1035, // ISRGRootX10 [10.1.0+]
	1036, // USERTrustEccCertificationAuthority [10.1.0+]
	1037, // COMODORsaCertificationAuthority [10.1.0+]
	1038, // COMODOEccCertificationAuthority [10.1.0+]
	1039, // AmazonRootCA2 [11.0.0+]
	1040, // AmazonRootCA3 [11.0.0+]
	1041, // AmazonRootCA4 [11.0.0+]
	1042, // DigiCertAssuredIDRootG3 [11.0.0+]
	1043, // DigiCertGlobalRootG3 [11.0.0+]
	1044, // DigiCertTrustedRootG4 [11.0.0+]
	1045, // EntrustRootCertificationAuthorityEC1 [11.0.0+]
	1046, // EntrustRootCertificationAuthorityG4 [11.0.0+]
	1047, // GlobalSignECCRootCAR4 [11.0.0+]
	1048, // GlobalSignECCRootCAR5 [11.0.0+]
	1049, // GlobalSignECCRootCAR6 [11.0.0+]
	1050, // GTSRootR1 [11.0.0+]
	1051, // GTSRootR2 [11.0.0+]
	1052, // GTSRootR3 [11.0.0+]
	1053, // GTSRootR4 [11.0.0+]
	1054, // SecurityCommunicationRootCA [12.0.0+]
	1055, // GlobalSignRootE4 [15.0.0+]
	1056, // GlobalSignRootR4 [15.0.0+]
	1057, // TTeleSecGlobalRootClass2 [15.0.0+]
	1058, // DigiCertTLSECCP384RootG5 [16.0.0+]
	1059, // DigiCertTLSRSA4096RootG5 [16.0.0+]
};

/**
 * Load CA certificates from the Switch's built-in SSL certificate store.
 * Uses libnx's ssl service to retrieve system CA certs in DER format,
 * then parses them into an mbedtls x509 certificate chain.
 *
 * Certificates are loaded one at a time to work around a libnx bug where
 * sslGetCertificates() with SslCaCertificateId_All fails due to a
 * bounds-check issue in the returned SslBuiltInCertificateInfo fixup.
 *
 * Returns 0 on success, non-zero on failure.
 */
static int nx_tls_load_ca_certs(nx_context_t *nx_ctx) {
	if (nx_ctx->ca_certs_loaded) {
		return 0;
	}

	mbedtls_x509_crt_init(&nx_ctx->ca_chain);

	Result rc = sslInitialize(1);
	if (R_FAILED(rc)) {
		fprintf(stderr, "sslInitialize() failed: 0x%x\n", rc);
		return -1;
	}

	int loaded = 0;
	int total = (int)(sizeof(nx_ca_cert_ids) / sizeof(nx_ca_cert_ids[0]));

	for (int i = 0; i < total; i++) {
		u32 id = nx_ca_cert_ids[i];
		u32 buf_size = 0;

		rc = sslGetCertificateBufSize(&id, 1, &buf_size);
		if (R_FAILED(rc)) {
			// Cert may not exist on this firmware version, skip it
			continue;
		}

		void *cert_buffer = malloc(buf_size);
		if (!cert_buffer) {
			continue;
		}

		u32 out_count = 0;
		rc = sslGetCertificates(cert_buffer, buf_size, &id, 1, &out_count);
		if (R_FAILED(rc)) {
			free(cert_buffer);
			continue;
		}

		SslBuiltInCertificateInfo *info = (SslBuiltInCertificateInfo *)cert_buffer;
		for (u32 j = 0; j < out_count; j++) {
			if (info[j].status != SslTrustedCertStatus_EnabledTrusted) {
				continue;
			}
			if (info[j].cert_data && info[j].cert_size > 0) {
				int ret = mbedtls_x509_crt_parse_der(
					&nx_ctx->ca_chain,
					info[j].cert_data,
					info[j].cert_size);
				if (ret == 0) {
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
	JSContext *ctx = req_cb->context;
	JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

	if (req->err) {
		/* Error during TLS handshake */
		char error_buf[100];
		mbedtls_strerror(req->err, error_buf, 100);
		args[0] = JS_NewError(ctx);
		JS_SetPropertyStr(ctx, args[0], "message",
						  JS_NewString(ctx, error_buf));
	} else {
		/* Handshake complete */
		args[1] = req_cb->buffer;
	}

	JSValue ret_val =
		JS_Call(ctx, req_cb->callback, JS_NULL, 2, args);
	JS_FreeValue(ctx, req_cb->buffer);
	JS_FreeValue(ctx, req_cb->callback);
	if (JS_IsException(ret_val)) {
		nx_emit_error_event(ctx);
	}
	JS_FreeValue(ctx, ret_val);
	js_free(ctx, req_cb);
	js_free(ctx, req);
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
		if (hostname) JS_FreeCString(ctx, hostname);
		JS_ThrowTypeError(ctx, "invalid input");
		return JS_EXCEPTION;
	}

	// argv[3] is the rejectUnauthorized flag (default: true)
	int reject_unauthorized = 1;
	if (argc > 3 && JS_IsBool(argv[3])) {
		reject_unauthorized = JS_ToBool(ctx, argv[3]);
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_tls_context_class_id);
	nx_tls_context_t *data = js_mallocz(ctx, sizeof(nx_tls_context_t));
	if (!data) {
		JS_FreeValue(ctx, obj);
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
		JS_FreeCString(ctx, hostname);
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}
	if (reject_unauthorized) {
		// Load system CA certs (lazy, once per context)
		if (nx_tls_load_ca_certs(nx_ctx) == 0) {
			mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
			mbedtls_ssl_conf_ca_chain(&data->conf, &nx_ctx->ca_chain, NULL);
		} else {
			// Failed to load certs — fall back to no verification with a warning
			fprintf(stderr, "Warning: failed to load system CA certs, TLS verification disabled\n");
			mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_NONE);
		}
	} else {
		mbedtls_ssl_conf_authmode(&data->conf, MBEDTLS_SSL_VERIFY_NONE);
	}
	mbedtls_ssl_conf_rng(&data->conf, mbedtls_ctr_drbg_random,
						 &nx_ctx->ctr_drbg);
	if ((ret = mbedtls_ssl_set_hostname(&data->ssl, hostname)) != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		JS_ThrowTypeError(ctx, "Failed setting hostname: %s", error_buf);
		JS_FreeCString(ctx, hostname);
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}
	mbedtls_ssl_set_bio(&data->ssl, &data->server_fd, mbedtls_net_send,
						mbedtls_net_recv, NULL);
	if ((ret = mbedtls_ssl_setup(&data->ssl, &data->conf)) != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		JS_ThrowTypeError(ctx, "Failed setting up SSL: %s", error_buf);
		JS_FreeCString(ctx, hostname);
		JS_FreeValue(ctx, obj);
		return JS_EXCEPTION;
	}

	JS_FreeCString(ctx, hostname);

	nx_tls_connect_t *req = js_malloc(ctx, sizeof(nx_tls_connect_t));
	nx_js_callback_t *req_cb = js_malloc(ctx, sizeof(nx_js_callback_t));
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
	js_free(ctx, req_cb);
	js_free(ctx, req);
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

	nx_tls_read_t *req = js_malloc(ctx, sizeof(nx_tls_read_t));
	nx_js_callback_t *req_cb = js_malloc(ctx, sizeof(nx_js_callback_t));
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
	js_free(ctx, req_cb);
	js_free(ctx, req);
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

	nx_tls_write_t *req = js_malloc(ctx, sizeof(nx_tls_write_t));
	nx_js_callback_t *req_cb = js_malloc(ctx, sizeof(nx_js_callback_t));
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
