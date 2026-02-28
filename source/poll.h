#pragma once
#include "queue.h"
#include <netinet/in.h>
#include <poll.h>
#include <stddef.h>

typedef struct nx_poll_s nx_poll_t;
typedef struct nx_read_s nx_read_t;
typedef struct nx_write_s nx_write_t;
typedef struct nx_connect_s nx_connect_t;
typedef struct nx_server_s nx_server_t;
typedef struct nx_watcher_s nx_watcher_t;
typedef struct nx_recvfrom_s nx_recvfrom_t;
typedef struct nx_sendto_s nx_sendto_t;

typedef void (*nx_watcher_cb)(nx_poll_t *p, nx_watcher_t *req, int revents);
typedef void (*nx_write_cb)(nx_poll_t *p, nx_write_t *req);
typedef void (*nx_read_cb)(nx_poll_t *p, nx_read_t *req);
typedef void (*nx_connect_cb)(nx_poll_t *p, nx_connect_t *req);
typedef void (*nx_server_cb)(nx_poll_t *p, nx_server_t *req, int client_fd);
typedef void (*nx_recvfrom_cb)(nx_poll_t *p, nx_recvfrom_t *req);
typedef void (*nx_sendto_cb)(nx_poll_t *p, nx_sendto_t *req);

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

struct nx_recvfrom_s {
	NX_WATCHER_FIELDS
	uint8_t *buffer;
	size_t buffer_size;
	size_t bytes_read;
	struct sockaddr_in remote_addr;
	nx_recvfrom_cb callback;
};

struct nx_sendto_s {
	NX_WATCHER_FIELDS
	const uint8_t *buffer;
	size_t buffer_size;
	size_t bytes_written;
	struct sockaddr_in dest_addr;
	nx_sendto_cb callback;
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
int nx_udp_new(nx_poll_t *p, nx_recvfrom_t *req, const char *ip, int port,
			   uint8_t *buffer, size_t buffer_size, nx_recvfrom_cb callback);
int nx_sendto(nx_poll_t *p, nx_sendto_t *req, int fd, const uint8_t *data,
			  size_t num_bytes, struct sockaddr_in *dest, nx_sendto_cb callback);

// Low-level API
int nx_add_watcher(nx_poll_t *p, nx_watcher_t *req);
int nx_remove_watcher(nx_poll_t *p, nx_watcher_t *req);
void nx_poll_init(nx_poll_t *p);
void nx_poll(nx_poll_t *p);
