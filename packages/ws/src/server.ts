import {
	CLOSED,
	computeAcceptKey,
	concat,
	maskData,
	parseFrameHeader,
} from './frame';
import { ServerWebSocket } from './websocket';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

export interface WebSocketServerOptions {
	/** Port to listen on. */
	port: number;
	/** IP address to bind to (defaults to `0.0.0.0`). */
	host?: string;
}

export interface ConnectionEventDetail {
	/** The connected WebSocket. */
	socket: ServerWebSocket;
	/** The original HTTP upgrade request. */
	request: Request;
}

/**
 * A WebSocket server that listens for incoming connections on a TCP port.
 *
 * Uses the Web `EventTarget` API and dispatches `CustomEvent` instances.
 *
 * @example
 * ```typescript
 * import { WebSocketServer } from '@nx.js/ws';
 *
 * const wss = new WebSocketServer({ port: 8080 });
 *
 * wss.addEventListener('connection', (e) => {
 *   const { socket, request } = e.detail;
 *   console.log('Client connected from', request.url);
 *
 *   socket.addEventListener('message', (ev) => {
 *     console.log('Received:', ev.data);
 *     socket.send(`Echo: ${ev.data}`);
 *   });
 *
 *   socket.addEventListener('close', () => {
 *     console.log('Client disconnected');
 *   });
 * });
 * ```
 */
export class WebSocketServer extends EventTarget {
	readonly clients = new Set<ServerWebSocket>();
	#server: ReturnType<typeof Switch.listen>;

	constructor(opts: WebSocketServerOptions) {
		super();
		const { port, host } = opts;

		this.#server = Switch.listen({
			port,
			ip: host,
			accept: (event) => {
				this.#handleConnection(event.socket);
			},
		});

		// Dispatch listening event on next microtask
		queueMicrotask(() => {
			this.dispatchEvent(new Event('listening'));
		});
	}

	/**
	 * Returns the address the server is listening on.
	 */
	address(): { port: number; host: string } | null {
		return null;
	}

	/**
	 * Close the server and all connected clients.
	 */
	close(): void {
		for (const client of this.clients) {
			client.close(1001, 'Server shutting down');
		}
		this.clients.clear();
		this.#server.close();
		this.dispatchEvent(new Event('close'));
	}

	async #handleConnection(socket: Switch.Socket): Promise<void> {
		try {
			const reader = socket.readable.getReader();
			let buffer = new Uint8Array(0);

			// Read headers until we find \r\n\r\n
			while (true) {
				const { done, value } = await reader.read();
				if (done) return;

				buffer = concat(buffer, value);

				const headerEnd = findHeaderEnd(buffer);
				if (headerEnd !== -1) {
					const headerBytes = buffer.slice(0, headerEnd);
					const remaining = buffer.slice(headerEnd + 4);
					reader.releaseLock();

					const headerStr = decoder.decode(headerBytes);
					const lines = headerStr.split('\r\n');
					const [method, path] = lines[0].split(' ');

					const headers = new Headers();
					for (let i = 1; i < lines.length; i++) {
						const col = lines[i].indexOf(':');
						if (col !== -1) {
							headers.set(
								lines[i].slice(0, col),
								lines[i].slice(col + 1).trim(),
							);
						}
					}

					const host = headers.get('host') || 'localhost';
					const request = new Request(`http://${host}${path}`, {
						method,
						headers,
					});

					// Validate WebSocket upgrade
					const upgrade = headers.get('upgrade');
					const key = headers.get('sec-websocket-key');

					if (!upgrade || upgrade.toLowerCase() !== 'websocket' || !key) {
						const writer = socket.writable.getWriter();
						await writer.write(
							encoder.encode(
								'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n',
							),
						);
						await writer.close();
						return;
					}

					// Compute accept key using the shared utility
					const acceptKey = await computeAcceptKey(key);

					// Send 101 Switching Protocols
					const writer = socket.writable.getWriter();
					const responseHeaders = [
						'HTTP/1.1 101 Switching Protocols',
						'Upgrade: websocket',
						'Connection: Upgrade',
						`Sec-WebSocket-Accept: ${acceptKey}`,
						'',
						'',
					].join('\r\n');
					await writer.write(encoder.encode(responseHeaders));
					writer.releaseLock();

					// Create the server-side WebSocket
					const ws = new ServerWebSocket(
						socket.writable.getWriter(),
						`ws://${host}${path}`,
					);
					this.clients.add(ws);
					ws._open();

					ws.addEventListener('close', () => {
						this.clients.delete(ws);
					});

					this.dispatchEvent(
						new CustomEvent<ConnectionEventDetail>('connection', {
							detail: { socket: ws, request },
						}),
					);

					// Start reading WebSocket frames
					this.#readFrames(socket, ws, remaining);
					return;
				}
			}
		} catch (err) {
			this.dispatchEvent(new CustomEvent('error', { detail: err }));
		}
	}

	async #readFrames(
		socket: Switch.Socket,
		ws: ServerWebSocket,
		initial: Uint8Array,
	): Promise<void> {
		const reader = socket.readable.getReader();
		let buffer = initial;

		try {
			while (true) {
				// Try to parse frames from the buffer
				while (true) {
					const header = parseFrameHeader(buffer);
					if (
						!header ||
						buffer.length < header.headerSize + header.payloadLength
					) {
						break;
					}

					let payload = buffer.slice(
						header.headerSize,
						header.headerSize + header.payloadLength,
					);
					buffer = buffer.slice(header.headerSize + header.payloadLength);

					// RFC 6455: client frames MUST be masked
					if (!header.masked || !header.maskKey) {
						ws.close(1002, 'Client frames must be masked');
						reader.releaseLock();
						return;
					}

					// Unmask the payload
					payload = maskData(payload, header.maskKey);

					ws._handleFrame(header.opcode, header.fin, payload);

					if (ws.readyState === CLOSED) {
						reader.releaseLock();
						return;
					}
				}

				const { done, value } = await reader.read();
				if (done) break;

				buffer = concat(buffer, value);
			}
		} catch (err) {
			ws.dispatchEvent(new Event('error'));
		} finally {
			reader.releaseLock();
			if (ws.readyState !== CLOSED) {
				this.clients.delete(ws);
				ws.dispatchEvent(
					new CloseEvent('close', {
						code: 1006,
						reason: '',
						wasClean: false,
					}),
				);
			}
		}
	}
}

function findHeaderEnd(data: Uint8Array): number {
	for (let i = 0; i < data.length - 3; i++) {
		if (
			data[i] === 13 &&
			data[i + 1] === 10 &&
			data[i + 2] === 13 &&
			data[i + 3] === 10
		) {
			return i;
		}
	}
	return -1;
}
