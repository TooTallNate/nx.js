/**
 * WebSocket frame opcodes per RFC 6455.
 */
export const enum Opcode {
	Continuation = 0x0,
	Text = 0x1,
	Binary = 0x2,
	Close = 0x8,
	Ping = 0x9,
	Pong = 0xa,
}

export interface Frame {
	fin: boolean;
	opcode: Opcode;
	masked: boolean;
	payload: Uint8Array;
}

/**
 * Parse a single WebSocket frame from a buffer.
 * Returns the parsed frame and the number of bytes consumed,
 * or `null` if the buffer does not contain a complete frame.
 */
export function parseFrame(
	data: Uint8Array,
): { frame: Frame; bytesConsumed: number } | null {
	if (data.length < 2) return null;

	const byte0 = data[0];
	const byte1 = data[1];

	const fin = (byte0 & 0x80) !== 0;
	const opcode = byte0 & 0x0f;
	const masked = (byte1 & 0x80) !== 0;
	let payloadLength = byte1 & 0x7f;
	let offset = 2;

	if (payloadLength === 126) {
		if (data.length < 4) return null;
		payloadLength = (data[2] << 8) | data[3];
		offset = 4;
	} else if (payloadLength === 127) {
		if (data.length < 10) return null;
		// For simplicity, only support up to 32-bit payload lengths
		payloadLength =
			(data[6] << 24) | (data[7] << 16) | (data[8] << 8) | data[9];
		offset = 10;
	}

	const maskKeyLength = masked ? 4 : 0;
	const totalLength = offset + maskKeyLength + payloadLength;
	if (data.length < totalLength) return null;

	let payload: Uint8Array;
	if (masked) {
		const maskKey = data.slice(offset, offset + 4);
		offset += 4;
		payload = data.slice(offset, offset + payloadLength);
		for (let i = 0; i < payload.length; i++) {
			payload[i] ^= maskKey[i & 3];
		}
	} else {
		payload = data.slice(offset, offset + payloadLength);
	}

	return {
		frame: { fin, opcode: opcode as Opcode, masked, payload },
		bytesConsumed: totalLength,
	};
}

/**
 * Serialize a WebSocket frame into bytes.
 * Server frames are NOT masked per RFC 6455.
 */
export function serializeFrame(
	opcode: Opcode,
	payload: Uint8Array,
	fin = true,
): Uint8Array {
	const length = payload.length;
	let headerSize = 2;
	if (length > 65535) headerSize += 8;
	else if (length > 125) headerSize += 2;

	const frame = new Uint8Array(headerSize + length);
	frame[0] = (fin ? 0x80 : 0) | opcode;

	if (length > 65535) {
		frame[1] = 127;
		// Write as 64-bit (upper 32 bits = 0 for our use case)
		frame[2] = 0;
		frame[3] = 0;
		frame[4] = 0;
		frame[5] = 0;
		frame[6] = (length >> 24) & 0xff;
		frame[7] = (length >> 16) & 0xff;
		frame[8] = (length >> 8) & 0xff;
		frame[9] = length & 0xff;
	} else if (length > 125) {
		frame[1] = 126;
		frame[2] = (length >> 8) & 0xff;
		frame[3] = length & 0xff;
	} else {
		frame[1] = length;
	}

	frame.set(payload, headerSize);
	return frame;
}
