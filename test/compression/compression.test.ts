/**
 * Compression Streams Conformance Tests
 *
 * Compares the nx.js CompressionStream / DecompressionStream implementation
 * (compiled for host via the C test binary) against Chrome's native
 * Compression Streams API via Playwright.
 *
 * Test categories:
 * 1. Roundtrip tests: compress + decompress, verify JSON output matches
 * 2. Interop tests: compress with nx.js, decompress in Chrome (and vice versa)
 * 3. zstd tests: nx.js-only (Chrome doesn't support zstd)
 */

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { execSync } from 'node:child_process';
import {
	readFileSync,
	readdirSync,
	mkdirSync,
	existsSync,
	writeFileSync,
} from 'node:fs';
import { join, basename } from 'node:path';
import { chromium, type Browser } from 'playwright';

// Directories
const ROOT = import.meta.dirname;
const FIXTURES_DIR = join(ROOT, 'fixtures');
const OUTPUT_DIR = join(ROOT, 'output');
const BUILD_DIR = join(ROOT, 'build');
const BINARY = join(BUILD_DIR, 'nxjs-compression-test');
const BRIDGE = join(BUILD_DIR, 'compression_api.js');

// Fixtures that use zstd (not supported by Chrome)
const NXJS_ONLY_FIXTURES = ['zstd-roundtrip'];

// Fixtures that produce interop data (compressedBase64)
const INTEROP_FIXTURES = ['gzip-interop', 'deflate-interop'];

function ensureDir(dir: string) {
	if (!existsSync(dir)) {
		mkdirSync(dir, { recursive: true });
	}
}

/**
 * Run a fixture through the nxjs-compression-test binary and return parsed JSON.
 */
function runWithNxjs(fixturePath: string, outputPath: string): unknown {
	execSync(
		`"${BINARY}" "${BRIDGE}" "${fixturePath}" "${outputPath}"`,
		{ timeout: 15000, stdio: ['pipe', 'pipe', 'pipe'] }
	);
	const raw = readFileSync(outputPath, 'utf-8');
	return JSON.parse(raw);
}

/**
 * Run a roundtrip fixture in Chrome via Playwright and return parsed JSON.
 *
 * Chrome fixtures use the native CompressionStream / DecompressionStream.
 */
async function runRoundtripInChrome(
	browser: Browser,
	fixtureCode: string
): Promise<unknown> {
	const page = await browser.newPage();
	try {
		await page.setContent('<!DOCTYPE html><html><body></body></html>');

		const result = await page.evaluate((code: string) => {
			return new Promise((resolve, reject) => {
				// Provide compress / decompress using native Compression Streams
				(globalThis as any).compress = async function (
					format: string,
					data: Uint8Array
				): Promise<Uint8Array> {
					const cs = new CompressionStream(format);
					const writer = cs.writable.getWriter();
					const reader = cs.readable.getReader();
					const chunks: Uint8Array[] = [];

					writer.write(data);
					writer.close();

					while (true) {
						const { done, value } = await reader.read();
						if (done) break;
						chunks.push(new Uint8Array(value));
					}

					const totalLen = chunks.reduce(
						(s, c) => s + c.length,
						0
					);
					const result = new Uint8Array(totalLen);
					let offset = 0;
					for (const chunk of chunks) {
						result.set(chunk, offset);
						offset += chunk.length;
					}
					return result;
				};

				(globalThis as any).decompress = async function (
					format: string,
					data: Uint8Array
				): Promise<Uint8Array> {
					const ds = new DecompressionStream(format);
					const writer = ds.writable.getWriter();
					const reader = ds.readable.getReader();
					const chunks: Uint8Array[] = [];

					writer.write(data);
					writer.close();

					while (true) {
						const { done, value } = await reader.read();
						if (done) break;
						chunks.push(new Uint8Array(value));
					}

					const totalLen = chunks.reduce(
						(s, c) => s + c.length,
						0
					);
					const result = new Uint8Array(totalLen);
					let offset = 0;
					for (const chunk of chunks) {
						result.set(chunk, offset);
						offset += chunk.length;
					}
					return result;
				};

				// Provide textEncode/textDecode
				(globalThis as any).textEncode = function (
					str: string
				): Uint8Array {
					return new TextEncoder().encode(str);
				};
				(globalThis as any).textDecode = function (
					bytes: Uint8Array
				): string {
					return new TextDecoder().decode(bytes);
				};

				// Provide base64 helpers
				(globalThis as any).toBase64 = function (
					bytes: Uint8Array
				): string {
					let binary = '';
					for (let i = 0; i < bytes.length; i++) {
						binary += String.fromCharCode(bytes[i]);
					}
					return btoa(binary);
				};
				(globalThis as any).fromBase64 = function (
					str: string
				): Uint8Array {
					const binary = atob(str);
					const bytes = new Uint8Array(binary.length);
					for (let i = 0; i < binary.length; i++) {
						bytes[i] = binary.charCodeAt(i);
					}
					return bytes;
				};

				(globalThis as any).__output = function (val: unknown) {
					resolve(val);
				};

				// Execute the fixture
				try {
					const fn = new Function(code);
					fn();
				} catch (e) {
					reject(e);
				}
			});
		}, fixtureCode);

		return result;
	} finally {
		await page.close();
	}
}

/**
 * Decompress base64-encoded data in Chrome using the native DecompressionStream.
 */
async function decompressInChrome(
	browser: Browser,
	format: string,
	compressedBase64: string
): Promise<string> {
	const page = await browser.newPage();
	try {
		await page.setContent('<!DOCTYPE html><html><body></body></html>');

		const result = await page.evaluate(
			async ({
				format,
				compressedBase64,
			}: {
				format: string;
				compressedBase64: string;
			}) => {
				// Decode base64
				const binary = atob(compressedBase64);
				const bytes = new Uint8Array(binary.length);
				for (let i = 0; i < binary.length; i++) {
					bytes[i] = binary.charCodeAt(i);
				}

				// Decompress
				const ds = new DecompressionStream(format);
				const writer = ds.writable.getWriter();
				const reader = ds.readable.getReader();
				const chunks: Uint8Array[] = [];

				writer.write(bytes);
				writer.close();

				while (true) {
					const { done, value } = await reader.read();
					if (done) break;
					chunks.push(new Uint8Array(value));
				}

				const totalLen = chunks.reduce((s, c) => s + c.length, 0);
				const result = new Uint8Array(totalLen);
				let offset = 0;
				for (const chunk of chunks) {
					result.set(chunk, offset);
					offset += chunk.length;
				}

				return new TextDecoder().decode(result);
			},
			{ format, compressedBase64 }
		);

		return result;
	} finally {
		await page.close();
	}
}

/**
 * Compress text in Chrome and return base64 of compressed data.
 */
async function compressInChrome(
	browser: Browser,
	format: string,
	text: string
): Promise<string> {
	const page = await browser.newPage();
	try {
		await page.setContent('<!DOCTYPE html><html><body></body></html>');

		const result = await page.evaluate(
			async ({ format, text }: { format: string; text: string }) => {
				const data = new TextEncoder().encode(text);
				const cs = new CompressionStream(format);
				const writer = cs.writable.getWriter();
				const reader = cs.readable.getReader();
				const chunks: Uint8Array[] = [];

				writer.write(data);
				writer.close();

				while (true) {
					const { done, value } = await reader.read();
					if (done) break;
					chunks.push(new Uint8Array(value));
				}

				const totalLen = chunks.reduce((s, c) => s + c.length, 0);
				const result = new Uint8Array(totalLen);
				let offset = 0;
				for (const chunk of chunks) {
					result.set(chunk, offset);
					offset += chunk.length;
				}

				// Return as base64
				let binary = '';
				for (let i = 0; i < result.length; i++) {
					binary += String.fromCharCode(result[i]);
				}
				return btoa(binary);
			},
			{ format, text }
		);

		return result;
	} finally {
		await page.close();
	}
}

/**
 * Decompress Chrome-compressed data with nx.js by running a small fixture.
 */
function decompressWithNxjs(
	format: string,
	compressedBase64: string,
	outputPath: string
): string {
	const fixtureCode = `(function() {
	var compressed = fromBase64("${compressedBase64}");
	decompress("${format}", compressed).then(function(decompressed) {
		__output({ text: textDecode(decompressed) });
	});
})();`;

	const fixturePath = join(OUTPUT_DIR, `_temp_reverse_${format}.js`);
	writeFileSync(fixturePath, fixtureCode);

	execSync(
		`"${BINARY}" "${BRIDGE}" "${fixturePath}" "${outputPath}"`,
		{ timeout: 15000, stdio: ['pipe', 'pipe', 'pipe'] }
	);
	const raw = readFileSync(outputPath, 'utf-8');
	const result = JSON.parse(raw);
	return result.text;
}

// ---- Test Suite ----

describe('Compression Streams conformance: nx.js vs Chrome', () => {
	let browser: Browser;

	beforeAll(async () => {
		ensureDir(OUTPUT_DIR);

		if (!existsSync(BINARY)) {
			throw new Error(
				`Compression test binary not found at ${BINARY}. Run 'cmake -B build && cmake --build build' first.`
			);
		}
		if (!existsSync(BRIDGE)) {
			throw new Error(
				`Compression API bridge not found at ${BRIDGE}. Run 'cmake -B build && cmake --build build' first.`
			);
		}

		browser = await chromium.launch({ args: ['--disable-gpu'] });
	});

	afterAll(async () => {
		await browser?.close();
	});

	// Discover fixtures
	const fixtures = readdirSync(FIXTURES_DIR)
		.filter((f) => f.endsWith('.js'))
		.sort();

	for (const fixture of fixtures) {
		const name = basename(fixture, '.js');
		const isNxjsOnly = NXJS_ONLY_FIXTURES.includes(name);
		const isInterop = INTEROP_FIXTURES.includes(name);

		if (isNxjsOnly) {
			// zstd: nx.js-only roundtrip test (Chrome doesn't support zstd)
			it(`${name}: nx.js roundtrip (zstd — Chrome unsupported)`, () => {
				const fixturePath = join(FIXTURES_DIR, fixture);
				const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.json`);

				const nxjsResult = runWithNxjs(
					fixturePath,
					nxjsOutputPath
				) as any;

				console.log(`  ${name}: nx.js =`, JSON.stringify(nxjsResult));

				// Verify roundtrip succeeded
				expect(nxjsResult.roundtripMatch).toBe(true);
				expect(nxjsResult.decompressedLength).toBe(
					nxjsResult.originalLength
				);
				expect(nxjsResult.compressedLength).toBeGreaterThan(0);
				expect(nxjsResult.compressedLength).toBeLessThan(
					nxjsResult.originalLength * 2
				);
			});
		} else if (isInterop) {
			// Interop test: compress with nx.js, decompress in Chrome & vice versa
			it(`${name}: nx.js → Chrome decompression`, async () => {
				const fixturePath = join(FIXTURES_DIR, fixture);
				const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.json`);

				const nxjsResult = runWithNxjs(
					fixturePath,
					nxjsOutputPath
				) as any;

				console.log(`  ${name}: nx.js compressed =`, {
					format: nxjsResult.format,
					compressedLength: nxjsResult.compressedLength,
				});

				// Chrome decompresses nx.js-compressed data
				const decompressedText = await decompressInChrome(
					browser,
					nxjsResult.format,
					nxjsResult.compressedBase64
				);

				expect(decompressedText).toBe(nxjsResult.originalText);
			});

			it(`${name}: Chrome → nx.js decompression`, async () => {
				const fixturePath = join(FIXTURES_DIR, fixture);
				const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.json`);

				const nxjsResult = runWithNxjs(
					fixturePath,
					nxjsOutputPath
				) as any;

				// Chrome compresses the same text
				const chromeCompressedBase64 = await compressInChrome(
					browser,
					nxjsResult.format,
					nxjsResult.originalText
				);

				console.log(
					`  ${name}: Chrome compressed base64 length =`,
					chromeCompressedBase64.length
				);

				// nx.js decompresses Chrome-compressed data
				const reverseOutputPath = join(
					OUTPUT_DIR,
					`nxjs-reverse-${name}.json`
				);
				const decompressedText = decompressWithNxjs(
					nxjsResult.format,
					chromeCompressedBase64,
					reverseOutputPath
				);

				expect(decompressedText).toBe(nxjsResult.originalText);
			});
		} else {
			// Standard roundtrip test: compare nx.js output vs Chrome output
			it(`${name}: matches Chrome`, async () => {
				const fixturePath = join(FIXTURES_DIR, fixture);
				const fixtureCode = readFileSync(fixturePath, 'utf-8');
				const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.json`);

				// Run with both engines
				const nxjsResult = runWithNxjs(fixturePath, nxjsOutputPath);
				const chromeResult = await runRoundtripInChrome(
					browser,
					fixtureCode
				);

				// Write Chrome output for debugging
				writeFileSync(
					join(OUTPUT_DIR, `chrome-${name}.json`),
					JSON.stringify(chromeResult, null, 2) + '\n'
				);

				console.log(
					`  ${name}: nx.js =`,
					JSON.stringify(nxjsResult)
				);
				console.log(
					`  ${name}: Chrome =`,
					JSON.stringify(chromeResult)
				);

				// Compare — we check structural match but ignore compressedLength
				// since different implementations may produce different compressed
				// sizes (different default levels, etc.)
				const nxjsObj = nxjsResult as any;
				const chromeObj = chromeResult as any;

				if (nxjsObj.roundtripMatch !== undefined) {
					// Single roundtrip test
					expect(nxjsObj.roundtripMatch).toBe(true);
					expect(chromeObj.roundtripMatch).toBe(true);
					expect(nxjsObj.output).toBe(chromeObj.output);
					expect(nxjsObj.originalLength).toBe(
						chromeObj.originalLength
					);
					expect(nxjsObj.decompressedLength).toBe(
						chromeObj.decompressedLength
					);
				} else {
					// Multi-format test (large-data)
					for (const fmt of Object.keys(nxjsObj)) {
						expect(nxjsObj[fmt].roundtripMatch).toBe(true);
						expect(chromeObj[fmt].roundtripMatch).toBe(true);
						expect(nxjsObj[fmt].originalLength).toBe(
							chromeObj[fmt].originalLength
						);
					}
				}
			});
		}
	}
});
