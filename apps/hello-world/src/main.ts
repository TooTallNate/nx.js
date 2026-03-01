// Test: DecompressionStream('zstd') with large single-frame data
// This tests whether nx.js decompresses the full zstd frame or stops early.

const log = (msg: string) => {
	console.log(msg);
	console.debug(msg);
};

const path = 'sdmc:/FINAL FANTASY III [01002E2014158000][v0] (0.73 GB).nsz';
const file = Switch.file(path);

log('=== DecompressionStream zstd test ===');
log(`File: ${path}`);
log(`File size: ${file.size}`);

// Parse PFS0 to find the NCZ entry
const header = await file.slice(0, 16).arrayBuffer();
const hView = new DataView(header);
const fileCount = hView.getUint32(4, true);
const stringTableSize = hView.getUint32(8, true);

log(`PFS0: ${fileCount} files, string table ${stringTableSize} bytes`);

const entriesBuf = await file.slice(16, 16 + fileCount * 24).arrayBuffer();
const stringTable = await file
	.slice(16 + fileCount * 24, 16 + fileCount * 24 + stringTableSize)
	.arrayBuffer();
const dataStart = 16 + fileCount * 24 + stringTableSize;
const decoder = new TextDecoder();

for (let i = 0; i < fileCount; i++) {
	const eView = new DataView(entriesBuf, i * 24, 24);
	const offset = Number(eView.getBigUint64(0, true));
	const size = Number(eView.getBigUint64(8, true));
	const nameOff = eView.getUint32(16, true);
	const nameBytes = new Uint8Array(stringTable, nameOff);
	const nullIdx = nameBytes.indexOf(0);
	const name = decoder.decode(nameBytes.subarray(0, nullIdx));

	if (!name.endsWith('.ncz')) continue;

	log(`NCZ: ${name}, size=${size}`);

	// The compressed zstd data starts at offset 0x40D0 within the NCZ entry
	const compressedOffset = dataStart + offset + 0x40d0;
	const compressedSize = size - 0x40d0;
	log(
		`Compressed data: offset=0x${compressedOffset.toString(16)}, size=${compressedSize} (${(compressedSize / 1024 / 1024).toFixed(1)} MB)`,
	);

	// Slice just the compressed data
	const compressedBlob = file.slice(
		compressedOffset,
		compressedOffset + compressedSize,
	);
	log(`Compressed blob size: ${compressedBlob.size}`);

	// Decompress with DecompressionStream
	log('Starting decompression...');
	const startTime = performance.now();

	const ds = new DecompressionStream('zstd' as CompressionFormat);
	const decompressedStream = compressedBlob
		.stream()
		.pipeThrough<Uint8Array>(ds);
	const reader = decompressedStream.getReader();

	let totalDecompressed = 0;
	let chunkCount = 0;
	let minChunk = Infinity;
	let maxChunk = 0;
	for (;;) {
		const { done, value } = await reader.read();
		if (done) break;
		const len = value.byteLength;
		totalDecompressed += len;
		if (len < minChunk) minChunk = len;
		if (len > maxChunk) maxChunk = len;
		chunkCount++;
		if (chunkCount % 500 === 0) {
			const elapsed = ((performance.now() - startTime) / 1000).toFixed(1);
			const rate = (
				totalDecompressed /
				1024 /
				1024 /
				parseFloat(elapsed)
			).toFixed(1);
			log(
				`  ${chunkCount} chunks, ${(totalDecompressed / 1024 / 1024).toFixed(1)} MB (${elapsed}s, ${rate} MB/s) chunk range: ${minChunk}-${maxChunk}`,
			);
		}
	}

	const elapsed = performance.now() - startTime;
	log(`Decompression complete:`);
	log(`  Chunks: ${chunkCount}`);
	log(
		`  Decompressed: ${totalDecompressed} bytes (${(totalDecompressed / 1024 / 1024).toFixed(1)} MB)`,
	);
	log(`  Expected: 762675200 bytes (727.3 MB)`);
	log(`  Time: ${(elapsed / 1000).toFixed(1)}s`);
	log(`  Match: ${totalDecompressed === 762675200}`);

	if (totalDecompressed < 762675200) {
		log(
			`  SHORT BY: ${762675200 - totalDecompressed} bytes (${((762675200 - totalDecompressed) / 1024 / 1024).toFixed(1)} MB)`,
		);
	}
	break;
}

log('');
log('Press + to exit...');
