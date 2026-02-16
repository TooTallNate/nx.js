(function() {
	// Generate a large repetitive string (compresses well)
	var text = "";
	for (var i = 0; i < 1000; i++) {
		text += "Line " + i + ": The quick brown fox jumps over the lazy dog.\n";
	}
	var input = textEncode(text);

	var formats = ["gzip", "deflate", "deflate-raw"];
	var results = {};
	var completed = 0;

	function processFormat(idx) {
		if (idx >= formats.length) {
			__output(results);
			return;
		}
		var fmt = formats[idx];
		compress(fmt, input).then(function(compressed) {
			return decompress(fmt, compressed).then(function(decompressed) {
				results[fmt] = {
					originalLength: input.length,
					compressedLength: compressed.length,
					ratio: +(compressed.length / input.length).toFixed(4),
					roundtripMatch: textDecode(decompressed) === text
				};
				processFormat(idx + 1);
			});
		});
	}

	processFormat(0);
})();
