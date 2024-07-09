#pragma once
#include "queue.h"

typedef struct nx_poll_s nx_poll_t;
typedef struct nx_read_s nx_read_t;
typedef struct nx_write_s nx_write_t;
typedef struct nx_connect_s nx_connect_t;
typedef struct nx_server_s nx_server_t;
typedef struct nx_watcher_s nx_watcher_t;

typedef void (*nx_watcher_cb)(nx_poll_t *p, nx_watcher_t *req, int revents);
typedef void (*nx_write_cb)(nx_poll_t *p, nx_write_t *req);
typedef void (*nx_read_cb)(nx_poll_t *p, nx_read_t *req);
typedef void (*nx_connect_cb)(nx_poll_t *p, nx_connect_t *req);
typedef void (*nx_server_cb)(nx_poll_t *p, nx_server_t *req, int client_fd);

#define NX_WATCHER_FIELDS                                                      \
	int fd;                                                                    \
	int events;                                                                \
	int err;                                                                   \
	nx_watcher_cb watcher_callback;                                            \
	void *opaque;                                                              \
	SLIST_ENTRY(nx_watcher_s)                                                  \
	next;

struct nx_watcher_s {
	NX_WATCHER_FIELDS
};

struct nx_read_s {
	NX_WATCHER_FIELDS
	const uint8_t *buffer;
	size_t buffer_size;
	size_t bytes_read;
	nx_read_cb callback;
};

struct nx_write_s {
	NX_WATCHER_FIELDS
	const uint8_t *buffer;
	size_t buffer_size;
	size_t bytes_written;
	nx_write_cb callback;
};

struct nx_connect_s {
	NX_WATCHER_FIELDS
	nx_connect_cb callback;
};

struct nx_server_s {
	NX_WATCHER_FIELDS
	nx_server_cb callback;
};

struct nx_poll_s {
	struct pollfd *poll_fds;
	nfds_t poll_fds_used;
	nfds_t poll_fds_size;
	SLIST_HEAD(slisthead, nx_watcher_s)
	watchers_head;
};

// High-level API
int nx_tcp_connect(nx_poll_t *p, nx_connect_t *req, const char *ip, int port,
				   nx_connect_cb callback);
int nx_tcp_server(nx_poll_t *p, nx_server_t *req, const char *ip, int port,
				  nx_server_cb callback);
int nx_read(nx_poll_t *p, nx_read_t *req, int fd, const uint8_t *buffer,
			size_t buffer_size, nx_read_cb callback);
int nx_write(nx_poll_t *p, nx_write_t *req, int fd, const uint8_t *data,
			 size_t num_bytes, nx_write_cb callback);

// Low-level API
int nx_add_watcher(nx_poll_t *p, nx_watcher_t *req);
int nx_remove_watcher(nx_poll_t *p, nx_watcher_t *req);
void nx_poll_init(nx_poll_t *p);
void nx_poll(nx_poll_t *p);
