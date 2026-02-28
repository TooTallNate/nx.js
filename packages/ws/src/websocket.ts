import {
	buildFrame,
	CLOSED,
	CLOSING,
	CONNECTING,
	decodeClosePayload,
	encodeClosePayload,
	OP_BINARY,
	OP_CLOSE,
	OP_CONTINUATION,
	OP_PING,
	OP_PONG,
	OP_TEXT,
	OPEN,
} from './frame';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

export interface WebSocketEventMap {
	open: Event;
	message: MessageEvent;
	close: CloseEvent;
	error: Event;
}

/**
 * Server-side WebSocket connection.
 *
 * Uses the Web `EventTarget` API (not Node.js `EventEmitter`).
 * Server frames are NOT masked per RFC 6455.
 */
export class ServerWebSocket extends EventTarget {
	static readonly CONNECTING = CONNECTING;
	static readonly OPEN = OPEN;
	static readonly CLOSING = CLOSING;
	static readonly CLOSED = CLOSED;

	readonly CONNECTING = CONNECTING;
	readonly OPEN = OPEN;
	readonly CLOSING = CLOSING;
	readonly CLOSED = CLOSED;

	#readyState = CONNECTING;
	#writer: WritableStreamDefaultWriter<Uint8Array>;
	#binaryType: BinaryType = 'blob';
	#bufferedAmount = 0;
	#fragments: Uint8Array[] = [];
	#fragmentOpcode: number | null = null;

	/** The URL of the connection (from the HTTP upgrade request). */
	readonly url: string;
	/** The protocol negotiated during the handshake. */
	readonly protocol: string;
	/** The extensions negotiated during the handshake. */
	readonly extensions: string;

	constructor(
		writer: WritableStreamDefaultWriter<Uint8Array>,
		url: string,
		protocol = '',
		extensions = '',
	) {
		super();
		this.#writer = writer;
		this.url = url;
		this.protocol = protocol;
		this.extensions = extensions;
	}

	get readyState(): number {
		return this.#readyState;
	}

	get bufferedAmount(): number {
		return this.#bufferedAmount;
	}

	get binaryType(): BinaryType {
		return this.#binaryType;
	}

	set binaryType(value: BinaryType) {
		if (value !== 'blob' && value !== 'arraybuffer') {
			throw new SyntaxError(`Invalid binaryType: ${value}`);
		}
		this.#binaryType = value;
	}

	/** @internal Mark the socket as open and dispatch the `open` event. */
	_open() {
		this.#readyState = OPEN;
		this.dispatchEvent(new Event('open'));
	}

	/**
	 * Send data over the WebSocket connection.
	 */
	send(data: string | ArrayBufferLike | Blob | ArrayBufferView): void {
		if (this.#readyState !== OPEN) {
			throw new DOMException('WebSocket is not open', 'InvalidStateError');
		}

		let payload: Uint8Array;
		let opcode: number;

		if (typeof data === 'string') {
			payload = encoder.encode(data);
			opcode = OP_TEXT;
		} else if (
			data instanceof ArrayBuffer ||
			data instanceof SharedArrayBuffer
		) {
			payload = new Uint8Array(data);
			opcode = OP_BINARY;
		} else if (ArrayBuffer.isView(data)) {
			payload = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
			opcode = OP_BINARY;
		} else {
			throw new TypeError(
				'Blob send is not supported; convert to ArrayBuffer first',
			);
		}

		// Server frames are NOT masked
		const frame = buildFrame(opcode, payload, false);
		this.#bufferedAmount += frame.length;
		this.#writer.write(frame).then(() => {
			this.#bufferedAmount -= frame.length;
		});
	}

	/**
	 * Initiate the closing handshake.
	 */
	close(code?: number, reason?: string): void {
		if (this.#readyState === CLOSING || this.#readyState === CLOSED) {
			return;
		}

		this.#readyState = CLOSING;
		const payload = encodeClosePayload(code, reason);
		const frame = buildFrame(OP_CLOSE, payload, false);
		this.#writer.write(frame);
	}

	/** @internal Process an incoming frame. */
	_handleFrame(opcode: number, fin: boolean, payload: Uint8Array): void {
		switch (opcode) {
			case OP_TEXT:
			case OP_BINARY: {
				if (!fin) {
					this.#fragmentOpcode = opcode;
					this.#fragments.push(payload);
					return;
				}
				this.#dispatchMessage(opcode, payload);
				break;
			}
			case OP_CONTINUATION: {
				this.#fragments.push(payload);
				if (fin) {
					const totalLen = this.#fragments.reduce((s, f) => s + f.length, 0);
					const assembled = new Uint8Array(totalLen);
					let offset = 0;
					for (const frag of this.#fragments) {
						assembled.set(frag, offset);
						offset += frag.length;
					}
					const msgOpcode = this.#fragmentOpcode!;
					this.#fragments = [];
					this.#fragmentOpcode = null;
					this.#dispatchMessage(msgOpcode, assembled);
				}
				break;
			}
			case OP_PING: {
				const pong = buildFrame(OP_PONG, payload, false);
				this.#writer.write(pong);
				break;
			}
			case OP_PONG:
				break;
			case OP_CLOSE: {
				const { code, reason } = decodeClosePayload(payload);
				if (this.#readyState === OPEN) {
					const closeFrame = buildFrame(OP_CLOSE, payload, false);
					this.#writer.write(closeFrame);
				}
				this.#readyState = CLOSED;
				this.#writer.close().catch(() => {});
				this.dispatchEvent(
					new CloseEvent('close', {
						code,
						reason,
						wasClean: true,
					}),
				);
				break;
			}
		}
	}

	#dispatchMessage(opcode: number, payload: Uint8Array) {
		let data: string | ArrayBuffer;
		if (opcode === OP_TEXT) {
			data = decoder.decode(payload);
		} else {
			data = payload.buffer.slice(
				payload.byteOffset,
				payload.byteOffset + payload.byteLength,
			);
		}
		this.dispatchEvent(new MessageEvent('message', { data }));
	}
}
