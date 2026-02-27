import { Opcode, parseFrame, serializeFrame } from './frame';

const encoder = new TextEncoder();
const decoder = new TextDecoder();

export const CONNECTING = 0;
export const OPEN = 1;
export const CLOSING = 2;
export const CLOSED = 3;

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
	#fragmentOpcode: Opcode | null = null;

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
			throw new SyntaxError(
				`Invalid binaryType: ${value}`,
			);
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
			throw new DOMException(
				'WebSocket is not open',
				'InvalidStateError',
			);
		}

		let payload: Uint8Array;
		let opcode: Opcode;

		if (typeof data === 'string') {
			payload = encoder.encode(data);
			opcode = Opcode.Text;
		} else if (data instanceof ArrayBuffer || data instanceof SharedArrayBuffer) {
			payload = new Uint8Array(data);
			opcode = Opcode.Binary;
		} else if (ArrayBuffer.isView(data)) {
			payload = new Uint8Array(
				data.buffer,
				data.byteOffset,
				data.byteLength,
			);
			opcode = Opcode.Binary;
		} else {
			// Blob — not commonly used on server side but handle gracefully
			throw new TypeError('Blob send is not supported; convert to ArrayBuffer first');
		}

		const frame = serializeFrame(opcode, payload);
		this.#bufferedAmount += frame.length;
		this.#writer.write(frame).then(() => {
			this.#bufferedAmount -= frame.length;
		});
	}

	/**
	 * Initiate the closing handshake.
	 */
	close(code?: number, reason?: string): void {
		if (
			this.#readyState === CLOSING ||
			this.#readyState === CLOSED
		) {
			return;
		}

		this.#readyState = CLOSING;

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

		const frame = serializeFrame(Opcode.Close, payload);
		this.#writer.write(frame);
	}

	/** @internal Process an incoming frame. */
	_handleFrame(opcode: Opcode, fin: boolean, payload: Uint8Array): void {
		switch (opcode) {
			case Opcode.Text:
			case Opcode.Binary: {
				if (!fin) {
					// Start of fragmented message
					this.#fragmentOpcode = opcode;
					this.#fragments.push(payload);
					return;
				}
				this.#dispatchMessage(opcode, payload);
				break;
			}
			case Opcode.Continuation: {
				this.#fragments.push(payload);
				if (fin) {
					// Reassemble
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
			case Opcode.Ping: {
				// Respond with pong
				const pong = serializeFrame(Opcode.Pong, payload);
				this.#writer.write(pong);
				break;
			}
			case Opcode.Pong:
				// Ignore unsolicited pongs
				break;
			case Opcode.Close: {
				let code = 1005;
				let reason = '';
				if (payload.length >= 2) {
					code = (payload[0] << 8) | payload[1];
					reason = decoder.decode(payload.slice(2));
				}
				if (this.#readyState === OPEN) {
					// Echo the close frame back
					const closeFrame = serializeFrame(Opcode.Close, payload);
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

	#dispatchMessage(opcode: Opcode, payload: Uint8Array) {
		let data: string | ArrayBuffer;
		if (opcode === Opcode.Text) {
			data = decoder.decode(payload);
		} else {
			data =
				this.#binaryType === 'arraybuffer'
					? payload.buffer.slice(
							payload.byteOffset,
							payload.byteOffset + payload.byteLength,
					  )
					: payload.buffer.slice(
							payload.byteOffset,
							payload.byteOffset + payload.byteLength,
					  );
		}
		this.dispatchEvent(new MessageEvent('message', { data }));
	}
}
