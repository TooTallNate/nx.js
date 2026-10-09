#include "inspector.h"
#include "error.h"
#include "types.h"

#include <v8-inspector.h>

#include <mbedtls/base64.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace v8;
namespace vi = v8_inspector;

// ===========================================================================
// The V8 inspector over the Chrome DevTools Protocol.
//
// V8 ships the inspector; what an embedder has to supply is a transport and a
// place for execution to stop. The second is the reason this lives in C++
// rather than in the runtime's TypeScript.
//
// When a breakpoint hits, V8 calls `runMessageLoopOnPause()` and expects the
// embedder to *block inside it*, servicing the debugger, until the protocol
// says to resume. JavaScript is stopped for that whole time, so a transport
// written in JS could not read the "resume" that would release it. Node solves
// this with a dedicated thread; that is not needed here, because nx.js speaks
// to sockets as raw file descriptors (see `tcp.cc`), so this can own an fd and
// poll it directly — inside the pause loop, and once per turn otherwise.
//
// The socket is deliberately kept off the libuv loop for the same reason:
// `uv_run()` is driven by the main loop, which is not running while paused.
// ===========================================================================

namespace {

constexpr int kContextGroupId = 1;

// ---------------------------------------------------------------------------
// Strings. The protocol is UTF-8; the inspector hands out UTF-16.
// ---------------------------------------------------------------------------

std::string utf16_to_utf8(const uint16_t *chars, size_t length) {
	std::string out;
	out.reserve(length);
	for (size_t i = 0; i < length; ++i) {
		uint32_t cp = chars[i];
		// Combine a surrogate pair into the code point it encodes.
		if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < length) {
			uint32_t low = chars[i + 1];
			if (low >= 0xdc00 && low <= 0xdfff) {
				cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
				++i;
			}
		}
		if (cp < 0x80) {
			out.push_back((char)cp);
		} else if (cp < 0x800) {
			out.push_back((char)(0xc0 | (cp >> 6)));
			out.push_back((char)(0x80 | (cp & 0x3f)));
		} else if (cp < 0x10000) {
			out.push_back((char)(0xe0 | (cp >> 12)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3f)));
			out.push_back((char)(0x80 | (cp & 0x3f)));
		} else {
			out.push_back((char)(0xf0 | (cp >> 18)));
			out.push_back((char)(0x80 | ((cp >> 12) & 0x3f)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3f)));
			out.push_back((char)(0x80 | (cp & 0x3f)));
		}
	}
	return out;
}

std::string to_utf8(const vi::StringView &view) {
	if (view.is8Bit())
		return std::string((const char *)view.characters8(), view.length());
	return utf16_to_utf8(view.characters16(), view.length());
}

std::string to_utf8(std::unique_ptr<vi::StringBuffer> buffer) {
	return buffer ? to_utf8(buffer->string()) : std::string();
}

// ---------------------------------------------------------------------------
// The WebSocket transport, and the small HTTP surface Chrome looks for first.
// ---------------------------------------------------------------------------

std::string base64(const uint8_t *data, size_t length) {
	size_t needed = 0;
	mbedtls_base64_encode(nullptr, 0, &needed, data, length);
	std::string out(needed, '\0');
	size_t written = 0;
	if (mbedtls_base64_encode((unsigned char *)out.data(), out.size(), &written,
	                          data, length) != 0)
		return std::string();
	out.resize(written);
	return out;
}

// The handshake reply is a fixed transform of the client's key.
std::string websocket_accept(const std::string &key) {
	static const char *kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	std::string joined = key + kGuid;
	uint8_t digest[SHA1_HASH_SIZE];
	// libnx's, the same one crypto.cc uses; mbedtls deprecated its one-shot.
	sha1CalculateHash(digest, joined.data(), joined.size());
	return base64(digest, sizeof(digest));
}

std::string header_value(const std::string &request, const char *name) {
	// Header names are case-insensitive, and clients disagree about casing.
	std::string lowered;
	lowered.reserve(request.size());
	for (char c : request)
		lowered.push_back((char)tolower((unsigned char)c));

	std::string needle = std::string("\r\n") + name + ":";
	size_t at = lowered.find(needle);
	if (at == std::string::npos)
		return std::string();
	size_t start = at + needle.size();
	size_t end = lowered.find("\r\n", start);
	if (end == std::string::npos)
		return std::string();
	std::string value = request.substr(start, end - start);
	size_t first = value.find_first_not_of(" \t");
	size_t last = value.find_last_not_of(" \t");
	if (first == std::string::npos)
		return std::string();
	return value.substr(first, last - first + 1);
}

void set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

class Transport {
  public:
	// Whether a debugger has completed the WebSocket handshake.
	bool attached() const { return client_fd_ >= 0 && handshaked_; }
	bool listening() const { return listen_fd_ >= 0; }
	uint16_t port() const { return port_; }

	bool start(uint16_t port, std::string *error) {
		if (listen_fd_ >= 0)
			return true;

		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			*error = "could not create the inspector socket";
			return false;
		}
		int one = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		if (bind(fd, (sockaddr *)&addr, sizeof(addr)) != 0) {
			close(fd);
			*error = "could not bind the inspector port";
			return false;
		}
		if (listen(fd, 1) != 0) {
			close(fd);
			*error = "could not listen on the inspector port";
			return false;
		}
		set_nonblocking(fd);
		listen_fd_ = fd;
		port_ = port;
		return true;
	}

	void stop() {
		drop_client();
		if (listen_fd_ >= 0) {
			close(listen_fd_);
			listen_fd_ = -1;
		}
	}

	void send_text(const std::string &payload) {
		if (!attached())
			return;

		// Server-to-client frames are never masked.
		std::string frame;
		frame.push_back((char)0x81); // FIN + text
		size_t n = payload.size();
		if (n < 126) {
			frame.push_back((char)n);
		} else if (n <= 0xffff) {
			frame.push_back((char)126);
			frame.push_back((char)((n >> 8) & 0xff));
			frame.push_back((char)(n & 0xff));
		} else {
			frame.push_back((char)127);
			for (int shift = 56; shift >= 0; shift -= 8)
				frame.push_back((char)((n >> shift) & 0xff));
		}
		frame.append(payload);
		write_all(frame);
	}

	// Reads whatever is available and returns the complete protocol messages
	// found. Blocks until something arrives when `block` is set, which is what
	// makes a paused isolate still answer the debugger.
	std::vector<std::string> poll_messages(bool block) {
		std::vector<std::string> messages;
		if (listen_fd_ < 0)
			return messages;

		pollfd fds[2];
		int count = 0;
		fds[count].fd = listen_fd_;
		fds[count].events = POLLIN;
		++count;
		if (client_fd_ >= 0) {
			fds[count].fd = client_fd_;
			fds[count].events = POLLIN;
			++count;
		}

		// A bounded wait rather than an indefinite one: a paused isolate should
		// still notice the debugger disconnecting, and a caller that asked to
		// block gets to re-check its own state between waits.
		int timeout = block ? 50 : 0;
		if (::poll(fds, count, timeout) <= 0)
			return messages;

		if (fds[0].revents & POLLIN)
			accept_client();

		for (int i = 1; i < count; ++i) {
			if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;
			if (!read_available()) {
				drop_client();
				return messages;
			}
		}

		if (!handshaked_)
			try_handshake();
		if (handshaked_)
			drain_frames(&messages);
		return messages;
	}

  private:
	void accept_client() {
		int fd = accept(listen_fd_, nullptr, nullptr);
		if (fd < 0)
			return;
		// One debugger at a time; a second would see the same session twice.
		if (client_fd_ >= 0) {
			close(fd);
			return;
		}
		set_nonblocking(fd);
		client_fd_ = fd;
		handshaked_ = false;
		in_.clear();
	}

	void drop_client() {
		// Only a client that completed the handshake was a debugger. The
		// discovery endpoints use this same socket and close as soon as they
		// have answered, and reporting those as a detach would release an app
		// that is waiting for a debugger — which is exactly what the target
		// list polling `/json` did.
		bool was_debugger = handshaked_;
		if (client_fd_ >= 0) {
			close(client_fd_);
			client_fd_ = -1;
		}
		handshaked_ = false;
		in_.clear();
		pending_.clear();
		if (was_debugger && on_detach_)
			on_detach_();
	}

	bool read_available() {
		char buf[4096];
		for (;;) {
			ssize_t n = recv(client_fd_, buf, sizeof(buf), 0);
			if (n > 0) {
				in_.append(buf, (size_t)n);
				continue;
			}
			if (n == 0)
				return false; // orderly close
			// Nothing more to read right now.
			return errno == EAGAIN || errno == EWOULDBLOCK;
		}
	}

	void write_all(const std::string &data) {
		size_t sent = 0;
		while (sent < data.size()) {
			ssize_t n = send(client_fd_, data.data() + sent, data.size() - sent, 0);
			if (n > 0) {
				sent += (size_t)n;
				continue;
			}
			if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				// The debugger is not draining fast enough; wait for room
				// rather than dropping a protocol message.
				pollfd p{client_fd_, POLLOUT, 0};
				if (::poll(&p, 1, 1000) <= 0)
					break;
				continue;
			}
			break;
		}
	}

	void try_handshake() {
		size_t end = in_.find("\r\n\r\n");
		if (end == std::string::npos)
			return;
		std::string request = in_.substr(0, end + 4);
		in_.erase(0, end + 4);

		std::string key = header_value(request, "sec-websocket-key");
		if (key.empty()) {
			// Not an upgrade: the discovery endpoints Chrome asks for first.
			serve_http(request);
			drop_client();
			return;
		}

		std::string accept_key = websocket_accept(key);
		std::string reply = "HTTP/1.1 101 Switching Protocols\r\n"
		                    "Upgrade: websocket\r\n"
		                    "Connection: Upgrade\r\n"
		                    "Sec-WebSocket-Accept: " +
		                    accept_key + "\r\n\r\n";
		write_all(reply);
		handshaked_ = true;
		if (on_attach_)
			on_attach_();
	}

	void serve_http(const std::string &request) {
		std::string body;
		if (request.find("GET /json/version") == 0) {
			body = "{\"Browser\":\"nx.js\",\"Protocol-Version\":\"1.3\"}";
		} else if (request.find("GET /json") == 0) {
			// Chrome lists targets here and then dials webSocketDebuggerUrl, so
			// that URL has to be one the client can reach. The address the
			// console binds to is not it — echo back whatever host the client
			// used to get here.
			std::string host = header_value(request, "host");
			if (host.empty())
				host = "127.0.0.1:" + std::to_string(port_);
			std::string ws = host + "/nxjs";
			// `devtoolsFrontendUrl` is what chrome://inspect actually opens
			// when "inspect" is clicked; without it the target is listed and
			// the button does nothing. `v8only=true` selects the frontend for
			// a bare V8 embedder rather than the one that expects a page.
			body = "[{\"description\":\"nx.js\",\"id\":\"nxjs\","
			       "\"title\":\"nx.js\",\"type\":\"node\","
			       "\"url\":\"file://\","
			       "\"devtoolsFrontendUrl\":\"devtools://devtools/bundled/"
			       "js_app.html?experiments=true&v8only=true&ws=" +
			       ws +
			       "\","
			       "\"devtoolsFrontendUrlCompat\":\"devtools://devtools/"
			       "bundled/inspector.html?experiments=true&v8only=true&ws=" +
			       ws +
			       "\","
			       "\"webSocketDebuggerUrl\":\"ws://" +
			       ws + "\"}]";
		} else {
			write_all("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
			return;
		}
		write_all("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
		          "Content-Length: " +
		          std::to_string(body.size()) + "\r\n\r\n" + body);
	}

	// Pulls every complete frame out of `in_`, reassembling fragments.
	void drain_frames(std::vector<std::string> *messages) {
		for (;;) {
			if (in_.size() < 2)
				return;
			const uint8_t *p = (const uint8_t *)in_.data();
			bool fin = (p[0] & 0x80) != 0;
			uint8_t opcode = p[0] & 0x0f;
			bool masked = (p[1] & 0x80) != 0;
			uint64_t length = p[1] & 0x7f;
			size_t offset = 2;

			if (length == 126) {
				if (in_.size() < offset + 2)
					return;
				length = ((uint64_t)p[2] << 8) | p[3];
				offset += 2;
			} else if (length == 127) {
				if (in_.size() < offset + 8)
					return;
				length = 0;
				for (int i = 0; i < 8; ++i)
					length = (length << 8) | p[offset + i];
				offset += 8;
			}

			uint8_t mask[4] = {0, 0, 0, 0};
			if (masked) {
				if (in_.size() < offset + 4)
					return;
				for (int i = 0; i < 4; ++i)
					mask[i] = p[offset + i];
				offset += 4;
			}

			if (in_.size() < offset + length)
				return; // frame still arriving

			std::string payload = in_.substr(offset, (size_t)length);
			if (masked) {
				for (size_t i = 0; i < payload.size(); ++i)
					payload[i] = (char)(payload[i] ^ mask[i % 4]);
			}
			in_.erase(0, offset + (size_t)length);

			if (opcode == 0x8) { // close
				drop_client();
				return;
			}
			if (opcode == 0x9) { // ping -> pong
				std::string pong;
				pong.push_back((char)0x8a);
				pong.push_back((char)payload.size());
				pong.append(payload);
				write_all(pong);
				continue;
			}
			if (opcode == 0xa) // pong
				continue;

			// Text (0x1) or a continuation of one (0x0).
			pending_.append(payload);
			if (fin) {
				messages->push_back(pending_);
				pending_.clear();
			}
		}
	}

  public:
	std::function<void()> on_attach_;
	std::function<void()> on_detach_;

  private:
	int listen_fd_ = -1;
	int client_fd_ = -1;
	bool handshaked_ = false;
	uint16_t port_ = 0;
	std::string in_;
	std::string pending_;
};

// ---------------------------------------------------------------------------
// The inspector itself.
// ---------------------------------------------------------------------------

class NxInspector;
NxInspector *g_inspector = nullptr;

class NxChannel : public vi::V8Inspector::Channel {
  public:
	explicit NxChannel(Transport *transport) : transport_(transport) {}

	void sendResponse(int, std::unique_ptr<vi::StringBuffer> message) override {
		transport_->send_text(to_utf8(std::move(message)));
	}
	void sendNotification(std::unique_ptr<vi::StringBuffer> message) override {
		transport_->send_text(to_utf8(std::move(message)));
	}
	void flushProtocolNotifications() override {}

  private:
	Transport *transport_;
};

class NxInspector : public vi::V8InspectorClient {
  public:
	NxInspector(Isolate *iso, Local<Context> context)
	    : iso_(iso), context_(iso, context), channel_(&transport_) {
		inspector_ = vi::V8Inspector::create(iso, this);
		vi::StringView name((const uint8_t *)"nx.js", 5);
		inspector_->contextCreated(
		    vi::V8ContextInfo(context, kContextGroupId, name));

		transport_.on_attach_ = [this] { connect_session(); };
		transport_.on_detach_ = [this] {
			session_.reset();
			// A debugger that goes away must not leave the app stopped.
			paused_ = false;
			waiting_ = false;
		};
	}

	bool start(uint16_t port, bool wait, std::string *error) {
		if (!transport_.start(port, error))
			return false;
		waiting_ = wait;
		// With `wait`, nothing else runs until a debugger has attached and
		// released us — the only way to break on the first line of an app.
		while (waiting_)
			poll(true);
		return true;
	}

	void stop() {
		session_.reset();
		transport_.stop();
		paused_ = false;
		waiting_ = false;
	}

	bool listening() const { return transport_.listening(); }
	bool attached() const { return transport_.attached(); }
	uint16_t port() const { return transport_.port(); }

	// One turn of the transport. Safe to call with no debugger attached, and
	// safe to call while paused — that is the whole point.
	void poll(bool block) {
		if (in_poll_)
			return;
		in_poll_ = true;
		std::vector<std::string> messages = transport_.poll_messages(block);
		for (const std::string &message : messages)
			dispatch(message);
		in_poll_ = false;
	}

	// --- V8InspectorClient -------------------------------------------------

	void runMessageLoopOnPause(int) override {
		if (nested_)
			return; // already inside a pause loop
		nested_ = true;
		paused_ = true;
		// JavaScript is stopped here. Only this loop is running, which is why
		// the transport cannot live in JS.
		while (paused_ && transport_.attached())
			poll(true);
		nested_ = false;
		paused_ = false;
	}

	void quitMessageLoopOnPause() override { paused_ = false; }

	void runIfWaitingForDebugger(int) override {
		// The break is scheduled here rather than when the socket handshake
		// completed, because a pause scheduled before the client has sent
		// `Debugger.enable` is dropped. By this message the domain is on, so
		// the app stops on its first statement and the debugger opens with
		// that line on screen — which is the point of waiting at all.
		if (waiting_ && session_) {
			vi::StringView reason((const uint8_t *)"Break on start", 14);
			vi::StringView details((const uint8_t *)"", 0);
			session_->schedulePauseOnNextStatement(reason, details);
		}
		waiting_ = false;
	}

	Local<Context> ensureDefaultContextInGroup(int) override {
		return context_.Get(iso_);
	}

  private:
	void connect_session() {
		vi::StringView state((const uint8_t *)"", 0);
		session_ = inspector_->connect(
		    kContextGroupId, &channel_, state,
		    vi::V8Inspector::kFullyTrusted,
		    waiting_ ? vi::V8Inspector::kWaitingForDebugger
		             : vi::V8Inspector::kNotWaitingForDebugger);

	}

	void dispatch(const std::string &message) {
		if (!session_)
			return;
		vi::StringView view((const uint8_t *)message.data(), message.size());
		session_->dispatchProtocolMessage(view);
	}

	Isolate *iso_;
	Global<Context> context_;
	Transport transport_;
	NxChannel channel_;
	std::unique_ptr<vi::V8Inspector> inspector_;
	std::unique_ptr<vi::V8InspectorSession> session_;
	bool paused_ = false;
	bool nested_ = false;
	bool waiting_ = false;
	bool in_poll_ = false;
};

// ---------------------------------------------------------------------------
// Bindings.
// ---------------------------------------------------------------------------

void nx_inspector_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();

	int port = 9229;
	bool wait = false;
	if (info.Length() > 0 && info[0]->IsObject()) {
		Local<Object> opts = info[0].As<Object>();
		Local<Value> v;
		if (opts->Get(context, nx_str(iso, "port")).ToLocal(&v) && v->IsNumber())
			port = (int)v.As<Number>()->Value();
		if (opts->Get(context, nx_str(iso, "wait")).ToLocal(&v))
			wait = v->BooleanValue(iso);
	}
	if (port < 1 || port > 65535) {
		nx_throw(iso, "Inspector port must be between 1 and 65535");
		return;
	}

	if (!g_inspector) {
		nx_throw(iso, "Inspector is unavailable in this build");
		return;
	}

	std::string error;
	if (!g_inspector->start((uint16_t)port, wait, &error)) {
		nx_throw(iso, error.c_str());
		return;
	}
	info.GetReturnValue().Set(Number::New(iso, g_inspector->port()));
}

void nx_inspector_stop(const FunctionCallbackInfo<Value> &info) {
	if (g_inspector)
		g_inspector->stop();
}

void nx_inspector_attached(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	info.GetReturnValue().Set(
	    Boolean::New(iso, g_inspector && g_inspector->attached()));
}

} // namespace

void nx_init_inspector(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "inspectorStart", nx_inspector_start);
	NX_SET_FUNC(init_obj, "inspectorStop", nx_inspector_stop);
	NX_SET_FUNC(init_obj, "inspectorAttached", nx_inspector_attached);

	// Created now, before the application's script is compiled, rather than
	// when the socket is opened. V8 only tracks scripts that were parsed while
	// an inspector existed, so an inspector built on demand can evaluate
	// expressions but cannot see — or breakpoint — a single line the app wrote.
	//
	// Nothing listens until `inspectorStart()`; this only costs the inspector
	// object itself.
	if (!g_inspector)
		g_inspector = new NxInspector(iso, iso->GetCurrentContext());
}

void nx_inspector_poll(Isolate *iso) {
	if (g_inspector && g_inspector->listening())
		g_inspector->poll(false);
}

void nx_inspector_shutdown() {
	if (!g_inspector)
		return;
	g_inspector->stop();
	delete g_inspector;
	g_inspector = nullptr;
}
