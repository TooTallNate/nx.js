import { ServerWebSocket } from './websocket';
import { Opcode, parseFrame } from './frame';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// SHA-1 hash via SubtleCrypto
async function sha1(data: Uint8Array): Promise<ArrayBuffer> {
	return crypto.subtle.digest('SHA-1', data);
}

function toBase64(buffer: ArrayBuffer): string {
	const bytes = new Uint8Array(buffer);
	let binary = '';
	for (let i = 0; i < bytes.length; i++) {
		binary += String.fromCharCode(bytes[i]);
	}
	return btoa(binary);
}

const WS_MAGIC = '258EAFA5-E914-47DA-95CA-5AB5A3CF4665';

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
		// The Switch.listen API doesn't expose address info directly,
		// so we return what was configured.
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
			// Read the HTTP upgrade request
			const reader = socket.readable.getReader();
			let buffer = new Uint8Array(0);

			// Read headers until we find \r\n\r\n
			while (true) {
				const { done, value } = await reader.read();
				if (done) return;

				const newBuf = new Uint8Array(buffer.length + value.length);
				newBuf.set(buffer);
				newBuf.set(value, buffer.length);
				buffer = newBuf;

				// Check for end of headers
				const headerEnd = findHeaderEnd(buffer);
				if (headerEnd !== -1) {
					const headerBytes = buffer.slice(0, headerEnd);
					const remaining = buffer.slice(headerEnd + 4); // skip \r\n\r\n
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

					if (
						!upgrade ||
						upgrade.toLowerCase() !== 'websocket' ||
						!key
					) {
						// Not a WebSocket request — send 400
						const writer = socket.writable.getWriter();
						await writer.write(
							encoder.encode(
								'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n',
							),
						);
						await writer.close();
						return;
					}

					// Compute accept key
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

					// Dispatch connection event
					this.dispatchEvent(
						new CustomEvent<ConnectionEventDetail>(
							'connection',
							{
								detail: { socket: ws, request },
							},
						),
					);

					// Start reading WebSocket frames
					this.#readFrames(socket, ws, remaining);
					return;
				}
			}
		} catch (err) {
			this.dispatchEvent(
				new CustomEvent('error', { detail: err }),
			);
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
					const result = parseFrame(buffer);
					if (!result) break;
					const { frame, bytesConsumed } = result;

					if (!frame.masked) {
						// RFC 6455: client frames MUST be masked
						ws.close(1002, 'Client frames must be masked');
						reader.releaseLock();
						return;
					}

					ws._handleFrame(
						frame.opcode as Opcode,
						frame.fin,
						frame.payload,
					);

					buffer = buffer.slice(bytesConsumed);

					// If the socket was closed, stop
					if (ws.readyState === ServerWebSocket.CLOSED) {
						reader.releaseLock();
						return;
					}
				}

				const { done, value } = await reader.read();
				if (done) break;

				const newBuf = new Uint8Array(buffer.length + value.length);
				newBuf.set(buffer);
				newBuf.set(value, buffer.length);
				buffer = newBuf;
			}
		} catch (err) {
			ws.dispatchEvent(new Event('error'));
		} finally {
			reader.releaseLock();
			if (ws.readyState !== ServerWebSocket.CLOSED) {
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

async function computeAcceptKey(key: string): Promise<string> {
	const hash = await sha1(encoder.encode(key + WS_MAGIC));
	return toBase64(hash);
}
