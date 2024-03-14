import { flushBytes, readUntilEol } from './util';
import { UnshiftableStream } from './unshiftable-readable-stream';

function bodyWithContentLength(
	unshiftable: UnshiftableStream,
	contentLengthVal: string,
): ReadableStream<Uint8Array> {
	let bytesRead = 0;
	const reader = unshiftable.readable.getReader();
	const contentLength = parseInt(contentLengthVal, 10);
	//console.log(`content-length: ${contentLength}`);
	return new ReadableStream<Uint8Array>({
		async pull(controller) {
			if (bytesRead < contentLength) {
				unshiftable.resume();
				const { done, value } = await reader.read();
				unshiftable.pause();
				if (done) {
					reader.releaseLock();
					controller.close();
					return;
				}
				const remainingLength = contentLength - bytesRead;
				const chunkLength = value.length;
				if (chunkLength <= remainingLength) {
					controller.enqueue(value);
					bytesRead += chunkLength;
				} else {
					// If the chunk is larger than needed, slice it to fit and unshift the rest back
					const neededPart = value.slice(0, remainingLength);
					const excessPart = value.slice(remainingLength);
					controller.enqueue(neededPart);
					unshiftable.unshift(excessPart);
					bytesRead += neededPart.length;
					reader.releaseLock();
					controller.close(); // Close the stream as we have read the required content length
				}
			} else {
				reader.releaseLock();
				controller.close(); // Close the stream if bytesRead is already equal to contentLength
			}
		},
		cancel() {
			reader.cancel();
		},
	});
}

function bodyWithChunkedEncoding(
	unshiftable: UnshiftableStream,
): ReadableStream<Uint8Array> {
	const reader = unshiftable.readable.getReader();
	return new ReadableStream<Uint8Array>({
		async pull(controller) {
			const numBytesHex = await readUntilEol(reader, unshiftable);
			const numBytes = parseInt(numBytesHex, 16);
			if (Number.isNaN(numBytes)) {
				return controller.error(
					new Error(`Invalid chunk size: ${numBytesHex}`),
				);
			}
			if (numBytes > 0) {
				await flushBytes(controller, numBytes, reader, unshiftable);
			}
			const empty = await readUntilEol(reader, unshiftable);
			if (empty) {
				return controller.error(
					new Error(`Expected \\r\\n after data chunk, received: ${empty}`),
				);
			}
			if (numBytes === 0) {
				// This is the final chunk
				reader.releaseLock();
				controller.close();
			}
		},

		cancel(reason) {
			if (reason) {
				reader.cancel(reason);
			} else {
				reader.cancel();
			}
		},
	});
}

export function bodyStream(
	unshiftable: UnshiftableStream,
	headers: Headers,
): ReadableStream<Uint8Array> {
	const contentLength = headers.get('content-length');
	if (typeof contentLength === 'string') {
		return bodyWithContentLength(unshiftable, contentLength);
	}

	const transferEncoding = headers.get('transfer-encoding')?.split(/\s*,\s*/);
	if (transferEncoding?.includes('chunked')) {
		return bodyWithChunkedEncoding(unshiftable);
	}

	// Identity transfer encoding - read until the end of the stream for the body
	const body = new TransformStream<Uint8Array, Uint8Array>();
	unshiftable.readable.pipeTo(body.writable).finally(() => {
		unshiftable.pause();
	});
	unshiftable.resume();
	return body.readable;
}
