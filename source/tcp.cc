// TCP over libuv. The TS layer (and the TLS module) work with raw socket fds
// and (err, value) callbacks, so we drive non-blocking BSD sockets via
// uv_poll_t for readiness rather than uv_tcp_t (which would hide the fd). This
// preserves the exact `$` contract that QuickJS+poll.c provided.
#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

using namespace v8;

namespace {

// Set a socket non-blocking.
void set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// libuv permits only ONE uv_poll_t per fd, but the TS/TLS layer issues
// concurrent operations on the same fd (e.g. a write while a read is pending
// during a fetch). The old poll.c handled this by keeping a single watcher set
// per fd with OR'd event interest. We mirror that here with a per-fd registry:
// one shared uv_poll_t per fd, plus a list of pending ops. The handle is
// started with the union of all ops' interest; on readiness we dispatch to the
// matching ops and recompute the interest (stopping/closing the handle when no
// ops remain).

struct fd_poll_t; // forward

// One pending operation (connect/read/write) on an fd.
struct op_t {
	fd_poll_t *owner; // the per-fd registry entry
	Isolate *iso;
	int fd;
	int events; // UV_READABLE or UV_WRITABLE this op is waiting on
	void (*on_ready)(op_t *op, int events); // dispatch handler
	Global<Function> callback;
	Global<Value> buffer; // keeps the ArrayBuffer alive
	uint8_t *buf;
	size_t buf_size;
	op_t *next;
};

// One shared poll handle per fd, owning the list of pending ops.
struct fd_poll_t {
	uv_poll_t poll;
	Isolate *iso;
	int fd;
	op_t *ops;       // singly-linked list of pending ops
	bool closing;    // poll handle is being uv_close()d
	bool dispatching; // inside poll_dispatch_cb (defer teardown until it ends)
};

// fd -> fd_poll_t registry. Kept tiny and simple (linked list); socket counts
// on the Switch are low. Single-threaded (loop thread only), so no locking.
struct fd_reg_node_t {
	fd_poll_t *entry;
	fd_reg_node_t *next;
};
fd_reg_node_t *g_tcp_fds = nullptr;

fd_poll_t *registry_find(int fd) {
	for (fd_reg_node_t *n = g_tcp_fds; n; n = n->next) {
		if (n->entry->fd == fd)
			return n->entry;
	}
	return nullptr;
}

void registry_remove(fd_poll_t *fp) {
	fd_reg_node_t **pp = &g_tcp_fds;
	while (*pp) {
		if ((*pp)->entry == fp) {
			fd_reg_node_t *dead = *pp;
			*pp = dead->next;
			delete dead;
			return;
		}
		pp = &(*pp)->next;
	}
}

void poll_dispatch_cb(uv_poll_t *handle, int status, int events);

// Tear down a per-fd poll entry (stop + async-close the handle, drop registry).
void fd_poll_destroy(fd_poll_t *fp) {
	if (fp->closing)
		return;
	fp->closing = true;
	uv_poll_stop(&fp->poll);
	registry_remove(fp);
	uv_close((uv_handle_t *)&fp->poll,
	         [](uv_handle_t *h) { delete static_cast<fd_poll_t *>(h->data); });
}

// Compute the union of interest across all ops and (re)start or stop the poll.
void fd_poll_refresh(fd_poll_t *fp) {
	if (fp->closing)
		return;
	int mask = 0;
	for (op_t *o = fp->ops; o; o = o->next)
		mask |= o->events;
	if (mask == 0) {
		// No more pending ops. If we're inside poll_dispatch_cb, DON'T tear
		// down now (the dispatch loop still dereferences `fp`); it will call
		// fd_poll_destroy() when it finishes. Otherwise tear down immediately.
		if (!fp->dispatching)
			fd_poll_destroy(fp);
		return;
	}
	uv_poll_start(&fp->poll, mask, poll_dispatch_cb);
}

// Get (or lazily create) the shared poll handle for `fd`.
fd_poll_t *fd_poll_get(Isolate *iso, int fd) {
	fd_poll_t *fp = registry_find(fd);
	if (fp)
		return fp;
	fp = new fd_poll_t();
	fp->iso = iso;
	fp->fd = fd;
	fp->ops = nullptr;
	fp->closing = false;
	fp->dispatching = false;
	if (uv_poll_init_socket(nx_ctx(iso)->loop, &fp->poll, fd) != 0) {
		delete fp;
		return nullptr;
	}
	fp->poll.data = fp;
	fd_reg_node_t *node = new fd_reg_node_t{fp, g_tcp_fds};
	g_tcp_fds = node;
	return fp;
}

// Register a new op on `fd` and ensure the shared poll covers its interest.
// Returns nullptr (and leaves an exception pending) if the fd can't be polled.
op_t *op_new(Isolate *iso, int fd, Local<Function> cb, int events,
             void (*on_ready)(op_t *, int)) {
	fd_poll_t *fp = fd_poll_get(iso, fd);
	if (!fp) {
		nx_throw(iso, "failed to poll socket");
		return nullptr;
	}
	op_t *op = new op_t();
	op->owner = fp;
	op->iso = iso;
	op->fd = fd;
	op->events = events;
	op->on_ready = on_ready;
	op->callback.Reset(iso, cb);
	op->next = fp->ops;
	fp->ops = op;
	fd_poll_refresh(fp);
	return op;
}

// Detach `op` from its fd's op list (without touching the poll handle yet).
void op_unlink(op_t *op) {
	fd_poll_t *fp = op->owner;
	op_t **pp = &fp->ops;
	while (*pp) {
		if (*pp == op) {
			*pp = op->next;
			return;
		}
		pp = &(*pp)->next;
	}
}

// Invoke the JS callback with (err, value) and tear down the op.
void op_finish(op_t *op, Local<Value> err, Local<Value> value) {
	Isolate *iso = op->iso;
	Local<Context> context = iso->GetCurrentContext();
	fd_poll_t *fp = op->owner;

	// Remove this op first so the callback (which may start a new op on the
	// same fd) sees a consistent list, then recompute poll interest.
	op_unlink(op);

	Local<Function> cb = op->callback.Get(iso);
	Local<Value> args[] = {err, value};
	op->callback.Reset();
	op->buffer.Reset();

	// Recompute interest now (covers the case where the JS callback does NOT
	// start a new op; if it does, fd_poll_get reuses the same handle).
	fd_poll_refresh(fp);

	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
	delete op;
}

// Shared readiness callback: dispatch to every op whose interest fired.
// Ops may finish (and unlink) during dispatch, so snapshot the list first.
void poll_dispatch_cb(uv_poll_t *handle, int status, int events) {
	fd_poll_t *fp = static_cast<fd_poll_t *>(handle->data);
	Isolate *iso = fp->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());

	// Snapshot current ops (callbacks may mutate fp->ops).
	int count = 0;
	for (op_t *o = fp->ops; o; o = o->next)
		count++;
	if (count == 0)
		return;
	op_t **snapshot = new op_t *[count];
	int i = 0;
	for (op_t *o = fp->ops; o; o = o->next)
		snapshot[i++] = o;

	// Mark dispatching so op_finish()->fd_poll_refresh() does NOT free `fp`
	// out from under this loop (it would be a use-after-free). We tear down at
	// the end instead.
	fp->dispatching = true;

	for (int j = 0; j < count; j++) {
		op_t *op = snapshot[j];
		// Skip ops that were already finished/unlinked by an earlier callback
		// in this same dispatch.
		bool still_pending = false;
		for (op_t *o = fp->ops; o; o = o->next) {
			if (o == op) {
				still_pending = true;
				break;
			}
		}
		if (!still_pending)
			continue;
		if (status < 0) {
			op->on_ready(op, status); // propagate error (negative)
		} else if (events & op->events) {
			op->on_ready(op, events);
		}
	}
	delete[] snapshot;

	// Done dispatching. If no ops remain (and a new op wasn't started by a
	// callback), tear the handle down now that it's safe.
	fp->dispatching = false;
	if (fp->ops == nullptr && !fp->closing) {
		fd_poll_destroy(fp);
	}
}

Local<Value> make_errno(Isolate *iso, int err) {
	Local<Object> e = Exception::Error(nx_str(iso, strerror(err))).As<Object>();
	return e;
}

// Invoke a JS callback now with (err, value), forwarding any thrown error.
void call_now(Isolate *iso, Local<Function> cb, Local<Value> err,
              Local<Value> value) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> args[] = {err, value};
	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
}

// ---- connect ----
// on_ready: `events` >= 0 means the fd is writable (connect resolved); a
// negative value carries a uv error status.
void connect_on_ready(op_t *op, int events) {
	Isolate *iso = op->iso;
	int err = 0;
	socklen_t len = sizeof(err);
	getsockopt(op->fd, SOL_SOCKET, SO_ERROR, &err, &len);
	if (events < 0)
		err = -events;
	if (err) {
		op_finish(op, make_errno(iso, err), Undefined(iso));
	} else {
		op_finish(op, Undefined(iso), Integer::New(iso, op->fd));
	}
}

void nx_tcp_connect(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = info[0].As<Function>();
	String::Utf8Value ip(iso, info[1]);
	int port = 0;
	if (!*ip || !info[2]->Int32Value(context).To(&port)) {
		nx_throw(iso, "invalid input");
		return;
	}
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		call_now(iso, cb, make_errno(iso, errno), Undefined(iso));
		return;
	}
	set_nonblocking(fd);
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, *ip, &addr.sin_addr);
	int r = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (r == 0) {
		// Immediate success (rare).
		call_now(iso, cb, Undefined(iso), Integer::New(iso, fd));
		return;
	}
	if (errno != EINPROGRESS) {
		int e = errno;
		close(fd);
		call_now(iso, cb, make_errno(iso, e), Undefined(iso));
		return;
	}
	op_t *op = op_new(iso, fd, cb, UV_WRITABLE, connect_on_ready);
	(void)op; // op_new throws + returns null on failure
}

// ---- read ----
void read_on_ready(op_t *op, int events) {
	Isolate *iso = op->iso;
	if (events < 0) {
		op_finish(op, make_errno(iso, -events), Undefined(iso));
		return;
	}
	ssize_t n = recv(op->fd, op->buf, op->buf_size, 0);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return; // spurious wakeup; keep polling
		op_finish(op, make_errno(iso, errno), Undefined(iso));
		return;
	}
	op_finish(op, Undefined(iso), Integer::New(iso, (int)n));
}

void nx_tcp_read(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = info[0].As<Function>();
	int fd = 0;
	if (!info[1]->Int32Value(context).To(&fd)) {
		nx_throw(iso, "invalid input");
		return;
	}
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	op_t *op = op_new(iso, fd, cb, UV_READABLE, read_on_ready);
	if (!op)
		return;
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
}

// ---- write ----
void write_on_ready(op_t *op, int events) {
	Isolate *iso = op->iso;
	if (events < 0) {
		op_finish(op, make_errno(iso, -events), Undefined(iso));
		return;
	}
	ssize_t n = send(op->fd, op->buf, op->buf_size, 0);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		op_finish(op, make_errno(iso, errno), Undefined(iso));
		return;
	}
	op_finish(op, Undefined(iso), Integer::New(iso, (int)n));
}

void nx_tcp_write(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = info[0].As<Function>();
	int fd = 0;
	if (!info[1]->Int32Value(context).To(&fd)) {
		nx_throw(iso, "invalid input");
		return;
	}
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	op_t *op = op_new(iso, fd, cb, UV_WRITABLE, write_on_ready);
	if (!op)
		return;
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
}

// ---- close ----
void nx_tcp_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	int fd = 0;
	if (!info[0]->Int32Value(context).To(&fd)) {
		nx_throw(iso, "invalid input");
		return;
	}
	// If a shared poll handle is still registered for this fd (e.g. JS closes
	// the socket while ops are pending), stop + close it and drop any pending
	// ops before closing the fd, so libuv never polls a closed descriptor.
	fd_poll_t *fp = registry_find(fd);
	if (fp && !fp->closing) {
		op_t *o = fp->ops;
		while (o) {
			op_t *next = o->next;
			o->callback.Reset();
			o->buffer.Reset();
			delete o;
			o = next;
		}
		fp->ops = nullptr;
		fp->closing = true;
		uv_poll_stop(&fp->poll);
		registry_remove(fp);
		uv_close((uv_handle_t *)&fp->poll,
		         [](uv_handle_t *h) { delete static_cast<fd_poll_t *>(h->data); });
	}
	if (close(fd)) {
		nx_throw_errno_error(iso, errno, "close");
	}
}

// ---- TCP server ----
struct server_t {
	uv_poll_t poll;
	Isolate *iso;
	int fd;
	Global<Function> callback;
	bool active;
};

void server_poll_cb(uv_poll_t *handle, int status, int events) {
	server_t *s = static_cast<server_t *>(handle->data);
	Isolate *iso = s->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());
	if (status < 0)
		return;
	int client_fd = accept(s->fd, NULL, NULL);
	if (client_fd < 0)
		return;
	set_nonblocking(client_fd);
	Local<Function> cb = s->callback.Get(iso);
	Local<Value> args[] = {Integer::New(iso, client_fd)};
	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(iso->GetCurrentContext(), Null(iso), 1, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
}

void nx_tcp_server_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	String::Utf8Value ip(iso, info[0]);
	int port = 0;
	if (!*ip || !info[1]->Int32Value(context).To(&port)) {
		nx_throw(iso, "invalid input");
		return;
	}
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		nx_throw_errno_error(iso, errno, "socket");
		return;
	}
	int opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, *ip, &addr.sin_addr);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
	    listen(fd, 128) < 0) {
		int e = errno;
		close(fd);
		nx_throw_errno_error(iso, e, "listen");
		return;
	}
	set_nonblocking(fd);
	Local<Object> obj = nx::NewWrapped(iso);
	server_t *s = new server_t();
	s->iso = iso;
	s->fd = fd;
	s->callback.Reset(iso, info[2].As<Function>());
	s->active = true;
	uv_poll_init_socket(nx_ctx(iso)->loop, &s->poll, fd);
	s->poll.data = s;
	uv_poll_start(&s->poll, UV_READABLE, server_poll_cb);
	nx::Wrap<server_t>(iso, obj, s, [](server_t *p) {
		// Finalizer: stop + close on GC. (Best-effort; explicit close preferred.)
		if (p->active) {
			uv_poll_stop(&p->poll);
			close(p->fd);
			p->active = false;
		}
		p->callback.Reset();
		// NOTE: handle close is async; leak the struct rather than free under GC.
	});
	info.GetReturnValue().Set(obj);
}

void nx_tcp_server_close(const FunctionCallbackInfo<Value> &info) {
	server_t *s = nx::Unwrap<server_t>(info.This());
	if (!s || !s->active)
		return;
	s->active = false;
	uv_poll_stop(&s->poll);
	s->callback.Reset();
	shutdown(s->fd, SHUT_RDWR);
	close(s->fd);
}

void nx_tcp_init_server(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_FUNC(proto, "close", nx_tcp_server_close, 0);
}

} // namespace

void nx_init_tcp(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "connect", nx_tcp_connect);
	NX_SET_FUNC(init_obj, "read", nx_tcp_read);
	NX_SET_FUNC(init_obj, "write", nx_tcp_write);
	NX_SET_FUNC(init_obj, "close", nx_tcp_close);
	NX_SET_FUNC(init_obj, "tcpServerInit", nx_tcp_init_server);
	NX_SET_FUNC(init_obj, "tcpServerNew", nx_tcp_server_new);
}
