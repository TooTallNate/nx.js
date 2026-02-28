/**
 * WebSocket protocol constants and frame utilities per RFC 6455.
 *
 * Shared between the client ({@link WebSocket}) and
 * server ({@link WebSocketServer}) implementations.
 *
 * @module
 */

const encoder = new TextEncoder();
const decoder = new TextDecoder();

// ---- Opcodes ----

/** Continuation frame. */
export const OP_CONTINUATION = 0x0;
/** Text data frame. */
export const OP_TEXT = 0x1;
/** Binary data frame. */
export const OP_BINARY = 0x2;
/** Connection close frame. */
export const OP_CLOSE = 0x8;
/** Ping frame. */
export const OP_PING = 0x9;
/** Pong frame. */
export const OP_PONG = 0xa;

// ---- Ready states ----

/** The connection has not yet been established. */
export const CONNECTING = 0;
/** The connection is established and communication is possible. */
export const OPEN = 1;
/** The connection is going through the closing handshake. */
export const CLOSING = 2;
/** The connection has been closed or could not be opened. */
export const CLOSED = 3;

// ---- Frame header ----

/**
 * Parsed WebSocket frame header.
 */
export interface FrameHeader {
	/** Whether this is the final fragment. */
	fin: boolean;
	/** The frame opcode. */
	opcode: number;
	/** Whether the payload is masked. */
	masked: boolean;
	/** The length of the payload in bytes. */
	payloadLength: number;
	/** The 4-byte mask key, or `null` if not masked. */
	maskKey: Uint8Array | null;
	/** The total size of the header in bytes (including mask key). */
	headerSize: number;
}

/**
 * Parse a WebSocket frame header from raw bytes.
 *
 * Returns `null` if the buffer does not contain enough data
 * for a complete header.
 */
export function parseFrameHeader(data: Uint8Array): FrameHeader | null {
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

// ---- Frame building ----

/**
 * XOR-mask (or unmask) a payload with a 4-byte mask key.
 */
export function maskData(data: Uint8Array, maskKey: Uint8Array): Uint8Array {
	const result = new Uint8Array(data.length);
	for (let i = 0; i < data.length; i++) {
		result[i] = data[i] ^ maskKey[i & 3];
	}
	return result;
}

/**
 * Build a WebSocket frame.
 *
 * @param opcode - The frame opcode.
 * @param payload - The payload data.
 * @param mask - Whether to mask the payload (required for client-to-server frames).
 */
export function buildFrame(
	opcode: number,
	payload: Uint8Array,
	mask: boolean,
): Uint8Array {
	let headerLen = 2;
	if (payload.length > 65535) headerLen += 8;
	else if (payload.length > 125) headerLen += 2;

	const maskKeyLen = mask ? 4 : 0;
	const frame = new Uint8Array(headerLen + maskKeyLen + payload.length);
	frame[0] = 0x80 | opcode; // FIN + opcode
	let offset = 1;

	const maskBit = mask ? 0x80 : 0;
	if (payload.length > 65535) {
		frame[offset++] = maskBit | 127;
		const view = new DataView(frame.buffer);
		view.setBigUint64(offset, BigInt(payload.length));
		offset += 8;
	} else if (payload.length > 125) {
		frame[offset++] = maskBit | 126;
		frame[offset++] = (payload.length >> 8) & 0xff;
		frame[offset++] = payload.length & 0xff;
	} else {
		frame[offset++] = maskBit | payload.length;
	}

	if (mask) {
		const maskKey = new Uint8Array(4);
		crypto.getRandomValues(maskKey);
		frame.set(maskKey, offset);
		offset += 4;
		const masked = maskData(payload, maskKey);
		frame.set(masked, offset);
	} else {
		frame.set(payload, offset);
	}

	return frame;
}

// ---- Close frame helpers ----

/**
 * Encode a close frame payload from a code and optional reason.
 */
export function encodeClosePayload(code?: number, reason?: string): Uint8Array {
	if (code === undefined) return new Uint8Array(0);
	const reasonBytes = reason ? encoder.encode(reason) : new Uint8Array(0);
	const payload = new Uint8Array(2 + reasonBytes.length);
	payload[0] = (code >> 8) & 0xff;
	payload[1] = code & 0xff;
	payload.set(reasonBytes, 2);
	return payload;
}

/**
 * Decode a close frame payload into a code and reason string.
 */
export function decodeClosePayload(payload: Uint8Array): {
	code: number;
	reason: string;
} {
	if (payload.length < 2) return { code: 1005, reason: '' };
	const code = (payload[0] << 8) | payload[1];
	const reason = payload.length > 2 ? decoder.decode(payload.slice(2)) : '';
	return { code, reason };
}

// ---- Utilities ----

/**
 * Concatenate two `Uint8Array`s.
 */
export function concat(a: Uint8Array, b: Uint8Array): Uint8Array {
	const c = new Uint8Array(a.length + b.length);
	c.set(a, 0);
	c.set(b, a.length);
	return c;
}

/**
 * The WebSocket magic GUID used in the opening handshake (RFC 6455 Section 4.2.2).
 */
export const WS_MAGIC = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

/**
 * Compute the `Sec-WebSocket-Accept` header value for a given key.
 *
 * Used by the server to generate the response, and by the client to validate it.
 */
export async function computeAcceptKey(key: string): Promise<string> {
	const data = encoder.encode(key + WS_MAGIC);
	const hash = await crypto.subtle.digest('SHA-1', data);
	return new Uint8Array(hash).toBase64();
}

// ---- Server-side WebSocket creation ----

/**
 * Well-known symbol used to signal the internal constructor path.
 * Using `Symbol.for` ensures it's the same symbol across all bundles
 * (the runtime bundle and user app bundles share the global symbol registry).
 */
export const INTERNAL_SYMBOL = Symbol.for('nx.ws.internal');

/**
 * Initialization options for creating a server-side WebSocket.
 */
export interface WebSocketInit {
	/** The writable stream writer for sending frames. */
	writer: WritableStreamDefaultWriter<Uint8Array>;
	/** The readable stream reader for receiving frames. */
	reader: ReadableStreamDefaultReader<Uint8Array>;
	/** The URL of the WebSocket connection. */
	url: string;
	/** The negotiated sub-protocol. */
	protocol: string;
	/** The negotiated extensions. */
	extensions: string;
	/** Any leftover bytes from the HTTP upgrade that belong to the WebSocket stream. */
	initialBuffer: Uint8Array;
	/** Whether to mask outgoing frames (true for client, false for server). */
	mask: boolean;
	/** Whether to require incoming frames to be masked (true for server). */
	requireMask: boolean;
	/** Called when the WebSocket is cleaned up (e.g. to close the underlying TCP socket). */
	onCleanup?: () => void;
}

/**
 * Create a pre-connected WebSocket instance for server-side use.
 *
 * The returned instance is a real `WebSocket` object (passes `instanceof`)
 * with its read loop already started. Server frames are NOT masked.
 *
 * Uses `globalThis.WebSocket` with the well-known `INTERNAL_SYMBOL` to
 * trigger the internal constructor path, avoiding any cross-bundle
 * registration issues.
 *
 * @internal
 */
export function createWebSocket(init: WebSocketInit): WebSocket {
	return new (globalThis as any).WebSocket(INTERNAL_SYMBOL, init);
}
