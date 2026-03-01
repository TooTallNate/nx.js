(function () {
	// Generate a large string (~500KB) that compresses well
	var text = '';
	for (var i = 0; i < 10000; i++) {
		text += 'Line ' + i + ': The quick brown fox jumps over the lazy dog.\n';
	}
	var input = textEncode(text);

	compress('zstd', input).then(function (compressed) {
		return decompress('zstd', compressed).then(function (decompressed) {
			__output({
				originalLength: input.length,
				compressedLength: compressed.length,
				decompressedLength: decompressed.length,
				ratio: +(compressed.length / input.length).toFixed(4),
				roundtripMatch: textDecode(decompressed) === text,
			});
		});
	});
})();
