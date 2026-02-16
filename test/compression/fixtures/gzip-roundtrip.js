(function() {
	var input = textEncode("Hello, World! This is a test of gzip compression in nx.js.");
	compress("gzip", input).then(function(compressed) {
		return decompress("gzip", compressed).then(function(decompressed) {
			var output = textDecode(decompressed);
			__output({
				originalLength: input.length,
				compressedLength: compressed.length,
				decompressedLength: decompressed.length,
				roundtripMatch: output === "Hello, World! This is a test of gzip compression in nx.js.",
				output: output
			});
		});
	});
})();
