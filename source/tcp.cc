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

// One-shot operation (connect/read/write): a uv_poll_t watching a single fd,
// plus the retained JS callback and (for read/write) the ArrayBuffer.
struct op_t {
	uv_poll_t poll;
	Isolate *iso;
	int fd;
	Global<Function> callback;
	Global<Value> buffer; // keeps the ArrayBuffer alive
	uint8_t *buf;
	size_t buf_size;
	bool active;
};

op_t *op_new(Isolate *iso, int fd, Local<Function> cb) {
	op_t *op = new op_t();
	op->iso = iso;
	op->fd = fd;
	op->callback.Reset(iso, cb);
	op->active = true;
	uv_poll_init_socket(nx_ctx(iso)->loop, &op->poll, fd);
	op->poll.data = op;
	return op;
}

// Invoke the JS callback with (err, value) and tear down the op.
void op_finish(op_t *op, Local<Value> err, Local<Value> value) {
	Isolate *iso = op->iso;
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = op->callback.Get(iso);
	Local<Value> args[] = {err, value};
	op->active = false;
	uv_poll_stop(&op->poll);
	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
	op->callback.Reset();
	op->buffer.Reset();
	// Close the poll handle; free the op in the close callback.
	uv_close((uv_handle_t *)&op->poll,
	         [](uv_handle_t *h) { delete static_cast<op_t *>(h->data); });
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
void connect_poll_cb(uv_poll_t *handle, int status, int events) {
	op_t *op = static_cast<op_t *>(handle->data);
	Isolate *iso = op->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());
	int err = 0;
	socklen_t len = sizeof(err);
	getsockopt(op->fd, SOL_SOCKET, SO_ERROR, &err, &len);
	if (status < 0)
		err = -status;
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
	op_t *op = op_new(iso, fd, cb);
	uv_poll_start(&op->poll, UV_WRITABLE, connect_poll_cb);
}

// ---- read ----
void read_poll_cb(uv_poll_t *handle, int status, int events) {
	op_t *op = static_cast<op_t *>(handle->data);
	Isolate *iso = op->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());
	if (status < 0) {
		op_finish(op, make_errno(iso, -status), Undefined(iso));
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
	op_t *op = op_new(iso, fd, cb);
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
	uv_poll_start(&op->poll, UV_READABLE, read_poll_cb);
}

// ---- write ----
void write_poll_cb(uv_poll_t *handle, int status, int events) {
	op_t *op = static_cast<op_t *>(handle->data);
	Isolate *iso = op->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());
	if (status < 0) {
		op_finish(op, make_errno(iso, -status), Undefined(iso));
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
	op_t *op = op_new(iso, fd, cb);
	op->buf = buf;
	op->buf_size = size;
	op->buffer.Reset(iso, info[2]);
	uv_poll_start(&op->poll, UV_WRITABLE, write_poll_cb);
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
