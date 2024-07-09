#pragma once
#include "types.h"
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>

typedef struct {
	mbedtls_net_context server_fd;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
} nx_tls_context_t;

typedef struct nx_tls_connect_s nx_tls_connect_t;
typedef struct nx_tls_read_s nx_tls_read_t;
typedef struct nx_tls_write_s nx_tls_write_t;

typedef void (*nx_tls_connect_cb)(nx_poll_t *p, nx_tls_connect_t *req);

struct nx_tls_connect_s {
	NX_WATCHER_FIELDS
	nx_tls_context_t *data;
	nx_tls_connect_cb callback;
};

struct nx_tls_read_s {
	NX_WATCHER_FIELDS
	nx_tls_context_t *data;
	unsigned char *buffer;
	size_t buffer_size;
};

struct nx_tls_write_s {
	NX_WATCHER_FIELDS
	nx_tls_context_t *data;
	const uint8_t *buffer;
	size_t buffer_size;
	size_t bytes_written;
};

void nx_init_tls(JSContext *ctx, JSValueConst init_obj);
