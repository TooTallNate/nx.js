const decoder = new TextDecoder();

export async function* lineIterator(
	readable: ReadableStreamDefaultReader<Uint8Array>,
) {
	let buffer = '';
	while (true) {
		const chunk = await readable.read();
		if (chunk.done) break;
		buffer += decoder.decode(chunk.value);
		const lines = buffer.split('\r\n');
		for (let i = 0; i < lines.length - 1; i++) {
			yield lines[i];
		}
		buffer = lines[lines.length - 1];
	}
	if (buffer) yield buffer;
}
