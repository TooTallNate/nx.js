(function() {
	var input = textEncode("Hello, World! This is a test of zstd compression in nx.js.");
	compress("zstd", input).then(function(compressed) {
		return decompress("zstd", compressed).then(function(decompressed) {
			var output = textDecode(decompressed);
			__output({
				originalLength: input.length,
				compressedLength: compressed.length,
				decompressedLength: decompressed.length,
				roundtripMatch: output === "Hello, World! This is a test of zstd compression in nx.js.",
				output: output
			});
		});
	});
})();
