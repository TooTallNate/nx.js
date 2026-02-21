(function() {
	var input = textEncode("The quick brown fox jumps over the lazy dog");
	compress("deflate", input).then(function(compressed) {
		__output({
			format: "deflate",
			originalText: "The quick brown fox jumps over the lazy dog",
			compressedBase64: toBase64(compressed),
			originalLength: input.length,
			compressedLength: compressed.length
		});
	});
})();
