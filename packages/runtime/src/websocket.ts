import {
	buildFrame,
	CLOSED,
	CLOSING,
	CONNECTING,
	computeAcceptKey,
	concat,
	decodeClosePayload,
	encodeClosePayload,
	maskData,
	OP_BINARY,
	OP_CLOSE,
	OP_CONTINUATION,
	OP_PING,
	OP_PONG,
	OP_TEXT,
	OPEN,
	parseFrameHeader,
	type WebSocketInit,
	INTERNAL_SYMBOL as WS_INTERNAL,
} from '../../ws/src/frame';
import { DOMException } from './dom-exception';
import { INTERNAL_SYMBOL } from './internal';
import { Blob } from './polyfills/blob';
import { Event } from './polyfills/event';
import { EventTarget } from './polyfills/event-target';
import { decoder } from './polyfills/text-decoder';
import { encoder } from './polyfills/text-encoder';
import { URL } from './polyfills/url';
import { connect, Socket } from './tcp';
import { def } from './utils';

export type BinaryType = 'blob' | 'arraybuffer';

export interface MessageEventInit extends EventInit {
	data?: any;
	origin?: string;
	lastEventId?: string;
	source?: any;
	ports?: any[];
}

export class MessageEvent extends Event {
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

export interface CloseEventInit extends EventInit {
	code?: number;
	reason?: string;
	wasClean?: boolean;
}

export class CloseEvent extends Event {
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
	return bytes.toBase64();
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
	#url: string = '';
	#binaryType: BinaryType = 'blob';
	#socket: Socket | null = null;
	#writer: WritableStreamDefaultWriter<Uint8Array> | null = null;
	#mask: boolean = true;
	#requireMask: boolean = false;
	#onCleanup: (() => void) | null = null;

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

		// Internal construction path for server-created WebSockets.
		// Uses a well-known symbol (Symbol.for) shared across bundles.
		if ((url as unknown) === WS_INTERNAL) {
			const init = protocols as unknown as WebSocketInit;
			this.#url = init.url;
			this.#protocol = init.protocol;
			this.#extensions = init.extensions;
			this.#writer = init.writer;
			this.#mask = init.mask;
			this.#requireMask = init.requireMask;
			this.#onCleanup = init.onCleanup ?? null;
			this.#readyState = OPEN;
			queueMicrotask(() => this.#fireEvent('open', new Event('open')));
			this.#readLoop(init.reader, init.initialBuffer);
			return;
		}

		const parsedUrl = new URL(String(url));

		// Normalize http/https to ws/wss per the current spec
		if (parsedUrl.protocol === 'http:') {
			parsedUrl.protocol = 'ws:';
		} else if (parsedUrl.protocol === 'https:') {
			parsedUrl.protocol = 'wss:';
		}

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
		redirectCount = 0,
	): Promise<void> {
		const MAX_REDIRECTS = 5;

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
					const headerBytes = encoder.encode(headerStr.slice(0, endIdx + 4));
					buffer = buffer.slice(headerBytes.length);
				}
			}

			// Parse response
			const lines = responseHeaders.split('\r\n');
			const statusLine = lines[0];
			const statusMatch = statusLine.match(/HTTP\/1\.1 (\d+)/);
			if (!statusMatch) {
				throw new Error(`WebSocket handshake failed: ${statusLine}`);
			}
			const statusCode = +statusMatch[1];

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

			// Handle redirects (301, 302, 303, 307, 308)
			if (statusCode >= 300 && statusCode < 400) {
				reader.releaseLock();
				writer.releaseLock();
				this.#socket = null;
				this.#writer = null;
				socket.close();

				if (redirectCount >= MAX_REDIRECTS) {
					throw new Error('Too many WebSocket redirects');
				}

				const location = resHeaders.get('location');
				if (!location) {
					throw new Error(
						`WebSocket redirect (${statusCode}) without Location header`,
					);
				}

				const redirectUrl = new URL(
					location,
					`${isSecure ? 'wss' : 'ws'}://${hostname}${port !== (isSecure ? 443 : 80) ? ':' + port : ''}${path}`,
				);

				const redirectScheme = redirectUrl.protocol;
				if (
					redirectScheme !== 'ws:' &&
					redirectScheme !== 'wss:' &&
					redirectScheme !== 'http:' &&
					redirectScheme !== 'https:'
				) {
					throw new Error(
						`WebSocket redirect to unsupported scheme: ${redirectScheme}`,
					);
				}

				const newIsSecure =
					redirectScheme === 'wss:' || redirectScheme === 'https:';
				const newHostname = redirectUrl.hostname;
				const newPort = +redirectUrl.port || (newIsSecure ? 443 : 80);
				const newPath = (redirectUrl.pathname || '/') + redirectUrl.search;

				redirectUrl.protocol = newIsSecure ? 'wss:' : 'ws:';
				redirectUrl.hash = '';
				this.#url = redirectUrl.href;

				return this.#connect(
					newHostname,
					newPort,
					newPath,
					newIsSecure,
					protocols,
					redirectCount + 1,
				);
			}

			if (statusCode !== 101) {
				throw new Error(`WebSocket handshake failed: ${statusLine}`);
			}

			// Validate Sec-WebSocket-Accept
			const expectedAccept = await computeAcceptKey(key);
			const actualAccept = resHeaders.get('sec-websocket-accept');
			if (actualAccept !== expectedAccept) {
				throw new Error(
					`Invalid Sec-WebSocket-Accept: expected ${expectedAccept}, got ${actualAccept}`,
				);
			}

			this.#protocol = resHeaders.get('sec-websocket-protocol') ?? '';
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
				const header = parseFrameHeader(buffer);
				if (
					!header ||
					buffer.length < header.headerSize + header.payloadLength
				) {
					const { value, done } = await reader.read();
					if (done) {
						const state = this.#readyState as number;
						if (state !== (CLOSED as number)) {
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

				// Enforce masking requirement (server requires client frames to be masked)
				if (this.#requireMask && !header.masked) {
					await this.#sendCloseFrame(1002, 'Client frames must be masked');
					this.#readyState = CLOSED;
					this.#cleanup();
					this.#fireEvent(
						'close',
						new CloseEvent('close', {
							code: 1002,
							reason: 'Client frames must be masked',
							wasClean: false,
						}),
					);
					return;
				}

				// Unmask if needed
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
					const { code, reason } = decodeClosePayload(payload);

					if (this.#readyState === OPEN) {
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
			if (this.#binaryType === 'arraybuffer') {
				data = payload.buffer.slice(
					payload.byteOffset,
					payload.byteOffset + payload.byteLength,
				);
			} else {
				data = new Blob([payload]);
			}
		}
		this.#fireEvent('message', new MessageEvent('message', { data }));
	}

	async #sendFrame(opcode: number, payload: Uint8Array) {
		if (!this.#writer) return;
		const frame = buildFrame(opcode, payload, this.#mask);
		try {
			await this.#writer.write(frame);
		} catch {
			// connection lost
		}
	}

	async #sendCloseFrame(code?: number, reason?: string) {
		const payload = encodeClosePayload(code, reason);
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
		}
		if (this.#onCleanup) {
			try {
				this.#onCleanup();
			} catch {
				// ignore
			}
			this.#onCleanup = null;
		}
		this.#writer = null;
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
			if (ArrayBuffer.isView(data)) {
				bytes = new Uint8Array(
					(data as DataView).buffer,
					(data as DataView).byteOffset,
					(data as DataView).byteLength,
				);
			} else {
				bytes = new Uint8Array(data);
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
					'The message must not be greater than 123 bytes.',
					'SyntaxError',
				);
			}
		}

		if (this.#readyState === CLOSING || this.#readyState === CLOSED) {
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

def(WebSocket);
