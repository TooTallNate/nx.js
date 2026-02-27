import { def } from './utils';
import { Socket, connect } from './tcp';
import { INTERNAL_SYMBOL } from './internal';
import { encoder } from './polyfills/text-encoder';
import { decoder } from './polyfills/text-decoder';
import { EventTarget } from './polyfills/event-target';
import { Event } from './polyfills/event';
import { Blob } from './polyfills/blob';

// WebSocket ready states
const CONNECTING = 0;
const OPEN = 1;
const CLOSING = 2;
const CLOSED = 3;

// WebSocket opcodes
const OP_CONTINUATION = 0x0;
const OP_TEXT = 0x1;
const OP_BINARY = 0x2;
const OP_CLOSE = 0x8;
const OP_PING = 0x9;
const OP_PONG = 0xa;

interface MessageEventInit extends EventInit {
	data?: any;
	origin?: string;
	lastEventId?: string;
	source?: any;
	ports?: any[];
}

class MessageEvent extends Event {
	readonly data: any;
	readonly origin: string;
	readonly lastEventId: string;
	readonly source: any;
	readonly ports: ReadonlyArray<any>;
	constructor(type: string, init?: MessageEventInit) {
		super(type, init);
		this.data = init?.data ?? null;
		this.origin = init?.origin ?? '';
		this.lastEventId = init?.lastEventId ?? '';
		this.source = init?.source ?? null;
		this.ports = init?.ports ?? [];
	}
}

interface CloseEventInit extends EventInit {
	code?: number;
	reason?: string;
	wasClean?: boolean;
}

class CloseEvent extends Event {
	readonly code: number;
	readonly reason: string;
	readonly wasClean: boolean;
	constructor(type: string, init?: CloseEventInit) {
		super(type, init);
		this.code = init?.code ?? 0;
		this.reason = init?.reason ?? '';
		this.wasClean = init?.wasClean ?? false;
	}
}

function generateKey(): string {
	const bytes = new Uint8Array(16);
	crypto.getRandomValues(bytes);
	return btoa(String.fromCharCode(...bytes));
}

function concat(a: Uint8Array, b: Uint8Array): Uint8Array {
	const c = new Uint8Array(a.length + b.length);
	c.set(a, 0);
	c.set(b, a.length);
	return c;
}

function maskData(data: Uint8Array, maskKey: Uint8Array): Uint8Array {
	const masked = new Uint8Array(data.length);
	for (let i = 0; i < data.length; i++) {
		masked[i] = data[i] ^ maskKey[i % 4];
	}
	return masked;
}

function buildFrame(opcode: number, payload: Uint8Array): Uint8Array {
	const maskKey = new Uint8Array(4);
	crypto.getRandomValues(maskKey);
	const masked = maskData(payload, maskKey);

	let headerLen = 2;
	if (payload.length > 65535) headerLen += 8;
	else if (payload.length > 125) headerLen += 2;

	const frame = new Uint8Array(headerLen + 4 + payload.length);
	frame[0] = 0x80 | opcode; // FIN + opcode
	let offset = 1;

	if (payload.length > 65535) {
		frame[offset++] = 0x80 | 127; // MASK + 127
		const view = new DataView(frame.buffer);
		view.setBigUint64(offset, BigInt(payload.length));
		offset += 8;
	} else if (payload.length > 125) {
		frame[offset++] = 0x80 | 126; // MASK + 126
		frame[offset++] = (payload.length >> 8) & 0xff;
		frame[offset++] = payload.length & 0xff;
	} else {
		frame[offset++] = 0x80 | payload.length; // MASK + length
	}

	frame.set(maskKey, offset);
	offset += 4;
	frame.set(masked, offset);
	return frame;
}

interface FrameHeader {
	fin: boolean;
	opcode: number;
	masked: boolean;
	payloadLength: number;
	maskKey: Uint8Array | null;
	headerSize: number;
}

function parseFrameHeader(data: Uint8Array): FrameHeader | null {
	if (data.length < 2) return null;

	const fin = !!(data[0] & 0x80);
	const opcode = data[0] & 0x0f;
	const masked = !!(data[1] & 0x80);
	let payloadLength = data[1] & 0x7f;
	let offset = 2;

	if (payloadLength === 126) {
		if (data.length < 4) return null;
		payloadLength = (data[2] << 8) | data[3];
		offset = 4;
	} else if (payloadLength === 127) {
		if (data.length < 10) return null;
		const view = new DataView(data.buffer, data.byteOffset);
		payloadLength = Number(view.getBigUint64(2));
		offset = 10;
	}

	let maskKey: Uint8Array | null = null;
	if (masked) {
		if (data.length < offset + 4) return null;
		maskKey = data.slice(offset, offset + 4);
		offset += 4;
	}

	return { fin, opcode, masked, payloadLength, maskKey, headerSize: offset };
}

/**
 * The `WebSocket` object provides the API for creating and managing a
 * WebSocket connection to a server, as well as for sending and receiving
 * data on the connection.
 *
 * @see https://developer.mozilla.org/docs/Web/API/WebSocket
 */
export class WebSocket extends EventTarget {
	static readonly CONNECTING = CONNECTING;
	static readonly OPEN = OPEN;
	static readonly CLOSING = CLOSING;
	static readonly CLOSED = CLOSED;

	readonly CONNECTING = CONNECTING;
	readonly OPEN = OPEN;
	readonly CLOSING = CLOSING;
	readonly CLOSED = CLOSED;

	#readyState: number = CONNECTING;
	#bufferedAmount: number = 0;
	#protocol: string = '';
	#extensions: string = '';
	#url: string;
	#binaryType: BinaryType = 'blob';
	#socket: Socket | null = null;
	#writer: WritableStreamDefaultWriter<Uint8Array> | null = null;

	// Event handler properties
	onopen: ((this: WebSocket, ev: Event) => any) | null = null;
	onmessage: ((this: WebSocket, ev: MessageEvent) => any) | null = null;
	onerror: ((this: WebSocket, ev: Event) => any) | null = null;
	onclose: ((this: WebSocket, ev: CloseEvent) => any) | null = null;

	/**
	 * Creates a new WebSocket connection to the given URL.
	 *
	 * @param url The URL to connect to. Must use `ws:` or `wss:` protocol.
	 * @param protocols Optional subprotocol(s) to request.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/WebSocket
	 */
	constructor(url: string | URL, protocols?: string | string[]) {
		super();

		const parsedUrl = new URL(String(url));
		if (parsedUrl.protocol !== 'ws:' && parsedUrl.protocol !== 'wss:') {
			throw new DOMException(
				`The URL's scheme must be either 'ws' or 'wss'. '${parsedUrl.protocol.slice(0, -1)}' is not allowed.`,
				'SyntaxError',
			);
		}

		// Normalize: strip fragment
		parsedUrl.hash = '';
		this.#url = parsedUrl.href;

		const isSecure = parsedUrl.protocol === 'wss:';
		const hostname = parsedUrl.hostname;
		const port = +parsedUrl.port || (isSecure ? 443 : 80);
		const path = (parsedUrl.pathname || '/') + parsedUrl.search;

		const requestedProtocols = protocols
			? Array.isArray(protocols)
				? protocols
				: [protocols]
			: [];

		// Validate protocols
		const seen = new Set<string>();
		for (const p of requestedProtocols) {
			if (seen.has(p)) {
				throw new DOMException(
					`The subprotocol '${p}' is duplicated.`,
					'SyntaxError',
				);
			}
			seen.add(p);
		}

		this.#connect(hostname, port, path, isSecure, requestedProtocols);
	}

	async #connect(
		hostname: string,
		port: number,
		path: string,
		isSecure: boolean,
		protocols: string[],
	) {
		try {
			const socket = new Socket(
				// @ts-expect-error Internal constructor
				INTERNAL_SYMBOL,
				{ hostname, port },
				{ secureTransport: isSecure ? 'on' : 'off', connect },
			);
			this.#socket = socket;

			await socket.opened;

			const writer = socket.writable.getWriter();
			this.#writer = writer;

			// Perform HTTP upgrade handshake
			const key = generateKey();
			let header = `GET ${path} HTTP/1.1\r\n`;
			header += `Host: ${hostname}${port !== (isSecure ? 443 : 80) ? ':' + port : ''}\r\n`;
			header += `Upgrade: websocket\r\n`;
			header += `Connection: Upgrade\r\n`;
			header += `Sec-WebSocket-Key: ${key}\r\n`;
			header += `Sec-WebSocket-Version: 13\r\n`;
			if (protocols.length > 0) {
				header += `Sec-WebSocket-Protocol: ${protocols.join(', ')}\r\n`;
			}
			header += `\r\n`;

			await writer.write(encoder.encode(header));

			// Read the handshake response
			const reader = socket.readable.getReader();
			let buffer = new Uint8Array(0);
			let headersComplete = false;
			let responseHeaders = '';

			while (!headersComplete) {
				const { value, done } = await reader.read();
				if (done) {
					throw new Error('Connection closed during handshake');
				}
				buffer = concat(buffer, value);

				// Check for end of headers
				const headerStr = decoder.decode(buffer);
				const endIdx = headerStr.indexOf('\r\n\r\n');
				if (endIdx !== -1) {
					responseHeaders = headerStr.slice(0, endIdx);
					headersComplete = true;
					// Keep leftover data
					const headerBytes = encoder.encode(
						headerStr.slice(0, endIdx + 4),
					);
					buffer = buffer.slice(headerBytes.length);
				}
			}

			// Parse response
			const lines = responseHeaders.split('\r\n');
			const statusLine = lines[0];
			const statusMatch = statusLine.match(/HTTP\/1\.1 (\d+)/);
			if (!statusMatch || statusMatch[1] !== '101') {
				throw new Error(`WebSocket handshake failed: ${statusLine}`);
			}

			// Parse headers
			const resHeaders = new Map<string, string>();
			for (let i = 1; i < lines.length; i++) {
				const col = lines[i].indexOf(':');
				if (col !== -1) {
					resHeaders.set(
						lines[i].slice(0, col).trim().toLowerCase(),
						lines[i].slice(col + 1).trim(),
					);
				}
			}

			// Validate Sec-WebSocket-Accept
			const expectedAccept = await computeAcceptKey(key);
			const actualAccept = resHeaders.get('sec-websocket-accept');
			if (actualAccept !== expectedAccept) {
				throw new Error(
					`Invalid Sec-WebSocket-Accept: expected ${expectedAccept}, got ${actualAccept}`,
				);
			}

			// Set negotiated protocol
			const negotiatedProtocol =
				resHeaders.get('sec-websocket-protocol') ?? '';
			this.#protocol = negotiatedProtocol;

			// Set extensions
			this.#extensions = resHeaders.get('sec-websocket-extensions') ?? '';

			this.#readyState = OPEN;
			this.#fireEvent('open', new Event('open'));

			// Start reading frames
			this.#readLoop(reader, buffer);
		} catch (err) {
			this.#readyState = CLOSED;
			this.#fireEvent('error', new Event('error'));
			this.#fireEvent(
				'close',
				new CloseEvent('close', {
					code: 1006,
					reason: '',
					wasClean: false,
				}),
			);
		}
	}

	async #readLoop(
		reader: ReadableStreamDefaultReader<Uint8Array>,
		initialBuffer: Uint8Array,
	) {
		let buffer = initialBuffer;
		let fragmentOpcode = 0;
		let fragmentPayload = new Uint8Array(0);

		try {
			while (this.#readyState === OPEN || this.#readyState === CLOSING) {
				// Try to parse a frame from buffer
				const header = parseFrameHeader(buffer);
				if (
					!header ||
					buffer.length < header.headerSize + header.payloadLength
				) {
					// Need more data
					const { value, done } = await reader.read();
					if (done) {
						if (this.#readyState !== CLOSED) {
							this.#readyState = CLOSED;
							this.#fireEvent(
								'close',
								new CloseEvent('close', {
									code: 1006,
									reason: '',
									wasClean: false,
								}),
							);
						}
						return;
					}
					buffer = concat(buffer, value);
					continue;
				}

				let payload = buffer.slice(
					header.headerSize,
					header.headerSize + header.payloadLength,
				);
				buffer = buffer.slice(header.headerSize + header.payloadLength);

				// Unmask if needed (server frames should not be masked, but handle it)
				if (header.masked && header.maskKey) {
					payload = maskData(payload, header.maskKey);
				}

				const opcode = header.opcode;

				if (opcode === OP_CONTINUATION) {
					fragmentPayload = concat(fragmentPayload, payload);
					if (header.fin) {
						this.#handleMessage(fragmentOpcode, fragmentPayload);
						fragmentPayload = new Uint8Array(0);
						fragmentOpcode = 0;
					}
				} else if (opcode === OP_TEXT || opcode === OP_BINARY) {
					if (header.fin) {
						this.#handleMessage(opcode, payload);
					} else {
						fragmentOpcode = opcode;
						fragmentPayload = payload;
					}
				} else if (opcode === OP_CLOSE) {
					let code = 1005;
					let reason = '';
					if (payload.length >= 2) {
						code = (payload[0] << 8) | payload[1];
						if (payload.length > 2) {
							reason = decoder.decode(payload.slice(2));
						}
					}

					if (this.#readyState === OPEN) {
						// Server initiated close - echo it back
						this.#readyState = CLOSING;
						await this.#sendCloseFrame(code, reason);
					}

					this.#readyState = CLOSED;
					this.#cleanup();
					this.#fireEvent(
						'close',
						new CloseEvent('close', {
							code,
							reason,
							wasClean: true,
						}),
					);
					return;
				} else if (opcode === OP_PING) {
					// Respond with pong
					await this.#sendFrame(OP_PONG, payload);
				} else if (opcode === OP_PONG) {
					// Ignore pong
				}
			}
		} catch (err) {
			if (this.#readyState !== CLOSED) {
				this.#readyState = CLOSED;
				this.#fireEvent('error', new Event('error'));
				this.#fireEvent(
					'close',
					new CloseEvent('close', {
						code: 1006,
						reason: '',
						wasClean: false,
					}),
				);
			}
		}
	}

	#handleMessage(opcode: number, payload: Uint8Array) {
		let data: any;
		if (opcode === OP_TEXT) {
			data = decoder.decode(payload);
		} else {
			// Binary
			if (this.#binaryType === 'arraybuffer') {
				data = payload.buffer.slice(
					payload.byteOffset,
					payload.byteOffset + payload.byteLength,
				);
			} else {
				data = new Blob([payload]);
			}
		}
		this.#fireEvent(
			'message',
			new MessageEvent('message', { data }),
		);
	}

	async #sendFrame(opcode: number, payload: Uint8Array) {
		if (!this.#writer) return;
		const frame = buildFrame(opcode, payload);
		try {
			await this.#writer.write(frame);
		} catch {
			// connection lost
		}
	}

	async #sendCloseFrame(code?: number, reason?: string) {
		let payload: Uint8Array;
		if (code !== undefined) {
			const reasonBytes = reason ? encoder.encode(reason) : new Uint8Array(0);
			payload = new Uint8Array(2 + reasonBytes.length);
			payload[0] = (code >> 8) & 0xff;
			payload[1] = code & 0xff;
			payload.set(reasonBytes, 2);
		} else {
			payload = new Uint8Array(0);
		}
		await this.#sendFrame(OP_CLOSE, payload);
	}

	#cleanup() {
		if (this.#socket) {
			try {
				this.#socket.close();
			} catch {
				// ignore
			}
			this.#socket = null;
			this.#writer = null;
		}
	}

	#fireEvent(handlerName: string, event: Event) {
		this.dispatchEvent(event);
		const handler = (this as any)[`on${handlerName}`];
		if (typeof handler === 'function') {
			handler.call(this, event);
		}
	}

	// Public API

	/**
	 * The current state of the connection.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/readyState
	 */
	get readyState(): number {
		return this.#readyState;
	}

	/**
	 * The number of bytes of data that have been queued using calls to `send()`
	 * but not yet transmitted to the network.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/bufferedAmount
	 */
	get bufferedAmount(): number {
		return this.#bufferedAmount;
	}

	/**
	 * The sub-protocol selected by the server.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/protocol
	 */
	get protocol(): string {
		return this.#protocol;
	}

	/**
	 * The extensions selected by the server.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/extensions
	 */
	get extensions(): string {
		return this.#extensions;
	}

	/**
	 * The absolute URL of the WebSocket.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/url
	 */
	get url(): string {
		return this.#url;
	}

	/**
	 * The type of binary data being received over the connection.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/binaryType
	 */
	get binaryType(): BinaryType {
		return this.#binaryType;
	}

	set binaryType(value: BinaryType) {
		if (value !== 'blob' && value !== 'arraybuffer') {
			throw new DOMException(
				`The provided value '${value}' is not a valid enum value of type BinaryType.`,
				'SyntaxError',
			);
		}
		this.#binaryType = value;
	}

	/**
	 * Enqueues the specified data to be transmitted to the server over the WebSocket connection.
	 *
	 * @param data The data to send: string, ArrayBuffer, ArrayBufferView, or Blob.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/send
	 */
	send(data: string | ArrayBufferLike | ArrayBufferView | Blob): void {
		if (this.#readyState === CONNECTING) {
			throw new DOMException(
				"Failed to execute 'send' on 'WebSocket': Still in CONNECTING state.",
				'InvalidStateError',
			);
		}
		if (this.#readyState !== OPEN) {
			return;
		}

		if (typeof data === 'string') {
			const bytes = encoder.encode(data);
			this.#bufferedAmount += bytes.length;
			this.#sendFrame(OP_TEXT, bytes).then(() => {
				this.#bufferedAmount -= bytes.length;
			});
		} else if (data instanceof Blob) {
			data.arrayBuffer().then((ab) => {
				const bytes = new Uint8Array(ab);
				this.#bufferedAmount += bytes.length;
				this.#sendFrame(OP_BINARY, bytes).then(() => {
					this.#bufferedAmount -= bytes.length;
				});
			});
		} else {
			let bytes: Uint8Array;
			if (data instanceof ArrayBuffer) {
				bytes = new Uint8Array(data);
			} else {
				bytes = new Uint8Array(
					data.buffer,
					data.byteOffset,
					data.byteLength,
				);
			}
			this.#bufferedAmount += bytes.length;
			this.#sendFrame(OP_BINARY, bytes).then(() => {
				this.#bufferedAmount -= bytes.length;
			});
		}
	}

	/**
	 * Closes the WebSocket connection or connection attempt, if any.
	 *
	 * @param code A numeric value indicating the status code. If not specified, 1000 is used.
	 * @param reason A human-readable string explaining why the connection is closing.
	 * @see https://developer.mozilla.org/docs/Web/API/WebSocket/close
	 */
	close(code?: number, reason?: string): void {
		if (code !== undefined) {
			if (code !== 1000 && (code < 3000 || code > 4999)) {
				throw new DOMException(
					`The code must be either 1000, or between 3000 and 4999. ${code} is neither.`,
					'InvalidAccessError',
				);
			}
		}
		if (reason !== undefined) {
			const encoded = encoder.encode(reason);
			if (encoded.length > 123) {
				throw new DOMException(
					"The message must not be greater than 123 bytes.",
					'SyntaxError',
				);
			}
		}

		if (
			this.#readyState === CLOSING ||
			this.#readyState === CLOSED
		) {
			return;
		}

		if (this.#readyState === CONNECTING) {
			this.#readyState = CLOSED;
			this.#cleanup();
			this.#fireEvent(
				'close',
				new CloseEvent('close', {
					code: 1006,
					reason: '',
					wasClean: false,
				}),
			);
			return;
		}

		this.#readyState = CLOSING;
		this.#sendCloseFrame(code ?? 1000, reason);
	}
}

async function computeAcceptKey(key: string): Promise<string> {
	const magic = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
	const data = encoder.encode(key + magic);
	const hash = await crypto.subtle.digest('SHA-1', data);
	return btoa(String.fromCharCode(...new Uint8Array(hash)));
}

def(WebSocket);
