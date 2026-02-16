(function() {
	var input = textEncode("Hello, World! This is a test of deflate-raw compression in nx.js.");
	compress("deflate-raw", input).then(function(compressed) {
		return decompress("deflate-raw", compressed).then(function(decompressed) {
			var output = textDecode(decompressed);
			__output({
				originalLength: input.length,
				compressedLength: compressed.length,
				decompressedLength: decompressed.length,
				roundtripMatch: output === "Hello, World! This is a test of deflate-raw compression in nx.js.",
				output: output
			});
		});
	});
})();
