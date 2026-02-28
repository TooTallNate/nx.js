// ../../packages/ws/dist/frame.js
var encoder = new TextEncoder();
var decoder = new TextDecoder();
function concat(a, b) {
  const c = new Uint8Array(a.length + b.length);
  c.set(a, 0);
  c.set(b, a.length);
  return c;
}
var WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
async function computeAcceptKey(key) {
  const data = encoder.encode(key + WS_MAGIC);
  const hash = await crypto.subtle.digest("SHA-1", data);
  return new Uint8Array(hash).toBase64();
}
var INTERNAL_SYMBOL = Symbol.for("nx.ws.internal");
function createWebSocket(init) {
  return new globalThis.WebSocket(INTERNAL_SYMBOL, init);
}

// ../../packages/ws/dist/server.js
var encoder2 = new TextEncoder();
var decoder2 = new TextDecoder();
var WebSocketServer = class extends EventTarget {
  clients = /* @__PURE__ */ new Set();
  #server;
  constructor(opts) {
    super();
    const { port, host } = opts;
    this.#server = Switch.listen({
      port,
      ip: host,
      accept: (event) => {
        this.#handleConnection(event.socket);
      }
    });
    queueMicrotask(() => {
      this.dispatchEvent(new Event("listening"));
    });
  }
  /**
   * Returns the address the server is listening on.
   */
  address() {
    return null;
  }
  /**
   * Close the server and all connected clients.
   */
  close() {
    for (const client of this.clients) {
      client.close(1001, "Server shutting down");
    }
    this.clients.clear();
    this.#server.close();
    this.dispatchEvent(new Event("close"));
  }
  async #handleConnection(socket) {
    try {
      const reader = socket.readable.getReader();
      let buffer = new Uint8Array(0);
      while (true) {
        const { done, value } = await reader.read();
        if (done)
          return;
        buffer = concat(buffer, value);
        const headerEnd = findHeaderEnd(buffer);
        if (headerEnd !== -1) {
          const headerBytes = buffer.slice(0, headerEnd);
          const remaining = buffer.slice(headerEnd + 4);
          reader.releaseLock();
          const headerStr = decoder2.decode(headerBytes);
          const lines = headerStr.split("\r\n");
          const [method, path] = lines[0].split(" ");
          const headers = new Headers();
          for (let i = 1; i < lines.length; i++) {
            const col = lines[i].indexOf(":");
            if (col !== -1) {
              headers.set(lines[i].slice(0, col), lines[i].slice(col + 1).trim());
            }
          }
          const host = headers.get("host") || "localhost";
          const request = new Request(`http://${host}${path}`, {
            method,
            headers
          });
          const upgrade = headers.get("upgrade");
          const key = headers.get("sec-websocket-key");
          if (!upgrade || upgrade.toLowerCase() !== "websocket" || !key) {
            const writer2 = socket.writable.getWriter();
            await writer2.write(encoder2.encode("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n"));
            await writer2.close();
            return;
          }
          const acceptKey = await computeAcceptKey(key);
          const writer = socket.writable.getWriter();
          const responseHeaders = [
            "HTTP/1.1 101 Switching Protocols",
            "Upgrade: websocket",
            "Connection: Upgrade",
            `Sec-WebSocket-Accept: ${acceptKey}`,
            "",
            ""
          ].join("\r\n");
          await writer.write(encoder2.encode(responseHeaders));
          writer.releaseLock();
          const ws = createWebSocket({
            writer: socket.writable.getWriter(),
            reader: socket.readable.getReader(),
            url: `ws://${host}${path}`,
            protocol: "",
            extensions: "",
            initialBuffer: remaining,
            mask: false,
            requireMask: true,
            onCleanup: () => socket.close()
          });
          this.clients.add(ws);
          ws.addEventListener("close", () => {
            this.clients.delete(ws);
          });
          this.dispatchEvent(new CustomEvent("connection", {
            detail: { socket: ws, request }
          }));
          return;
        }
      }
    } catch (err) {
      this.dispatchEvent(new CustomEvent("error", { detail: err }));
    }
  }
};
function findHeaderEnd(data) {
  for (let i = 0; i < data.length - 3; i++) {
    if (data[i] === 13 && data[i + 1] === 10 && data[i + 2] === 13 && data[i + 3] === 10) {
      return i;
    }
  }
  return -1;
}

// src/main.ts
var wss = new WebSocketServer({ port: 8080 });
wss.addEventListener("listening", () => {
  console.log("WebSocket server is listening on port 8080");
});
wss.addEventListener("connection", (e) => {
  const { socket, request } = e.detail;
  console.log("Client connected from", request.url);
  socket.addEventListener("message", (ev) => {
    console.log("Received:", ev.data);
    socket.send(`Echo: ${ev.data}`);
  });
  socket.addEventListener("close", (ev) => {
    console.log("Client disconnected", ev.code, ev.reason);
  });
});
//# sourceMappingURL=main.js.map
