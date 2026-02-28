# `@nx.js/ws`

WebSocket server for [nx.js](https://nxjs.n8.io).

Provides a `WebSocketServer` class inspired by the [`ws`](https://github.com/websockets/ws) npm package, but using the Web `EventTarget` API instead of Node.js `EventEmitter`.

## Usage

```typescript
import { WebSocketServer } from '@nx.js/ws';

const wss = new WebSocketServer({ port: 8080 });

wss.addEventListener('listening', () => {
  console.log('WebSocket server is listening on port 8080');
});

wss.addEventListener('connection', (e) => {
  const { socket, request } = e.detail;
  console.log('Client connected');

  socket.addEventListener('message', (ev) => {
    console.log('Received:', ev.data);
    socket.send(`Echo: ${ev.data}`);
  });

  socket.addEventListener('close', (ev) => {
    console.log('Client disconnected', ev.code, ev.reason);
  });
});
```

## API

### `WebSocketServer`

- **Constructor:** `new WebSocketServer({ port, host? })`
- **Events:** `connection`, `listening`, `close`, `error`
- **Properties:** `clients` (Set of connected `ServerWebSocket` instances)
- **Methods:** `close()`, `address()`

### `ServerWebSocket`

- **Events:** `open`, `message`, `close`, `error`
- **Properties:** `readyState`, `bufferedAmount`, `binaryType`, `url`, `protocol`, `extensions`
- **Methods:** `send(data)`, `close(code?, reason?)`

## Protocol Compliance

- Implements RFC 6455 WebSocket protocol
- Server frames are NOT masked (per spec)
- Client frames MUST be masked (validated; connection closed with 1002 if not)
- Handles ping/pong automatically
- Supports fragmented messages
- Supports text and binary frames
