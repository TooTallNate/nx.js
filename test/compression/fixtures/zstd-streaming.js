(function () {
	// Test multi-chunk zstd streaming decompression.
	// This simulates what happens when blob.stream() feeds 64KB chunks
	// through DecompressionStream — the same pattern that was causing
	// OOM on the Nintendo Switch.

	// Generate ~10MB of data that doesn't compress too well,
	// so the compressed output exceeds 64KB and requires multiple
	// write chunks (simulating what happens with real NCZ files).
	var parts = [];
	for (var i = 0; i < 200000; i++) {
		// Mix in the index to reduce compression ratio
		parts.push(
			'Line ' +
				i +
				': ' +
				Math.random().toString(36).substring(2, 15) +
				' The quick brown fox jumps over the lazy dog.\n',
		);
	}
	var text = parts.join('');
	var input = textEncode(text);

	// Compress in one shot to get the compressed payload
	compress('zstd', input).then(function (compressed) {
		// Now decompress by feeding in small chunks (simulating blob.stream())
		var CHUNK_SIZE = 65536; // 64KB, same as FsFile.stream()
		var ds = new DecompressionStream('zstd');
		var writer = ds.writable.getWriter();
		var reader = ds.readable.getReader();

		// Write compressed data in chunks
		var writeOffset = 0;
		var chunkCount = 0;

		function writeNextChunk() {
			if (writeOffset >= compressed.length) {
				writer.close();
				return;
			}
			var end = Math.min(writeOffset + CHUNK_SIZE, compressed.length);
			var chunk = compressed.slice(writeOffset, end);
			writeOffset = end;
			chunkCount++;
			writer.write(chunk).then(writeNextChunk);
		}

		writeNextChunk();

		// Read all decompressed output
		var decompressedChunks = [];
		var totalDecompressed = 0;
		var readChunkCount = 0;

		function readNext() {
			return reader.read().then(function (r) {
				if (r.done) {
					var output = concatUint8Arrays(decompressedChunks);
					__output({
						originalLength: input.length,
						compressedLength: compressed.length,
						decompressedLength: output.length,
						writeChunks: chunkCount,
						readChunks: readChunkCount,
						roundtripMatch: textDecode(output) === text,
					});
					return;
				}
				decompressedChunks.push(r.value);
				totalDecompressed += r.value.length;
				readChunkCount++;
				return readNext();
			});
		}

		readNext();
	});
})();
