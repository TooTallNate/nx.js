/**
 * WebAssembly Conformance Tests
 *
 * Compares the nx.js WebAssembly implementation (compiled for host via the C
 * test binary) against Chrome's native WebAssembly via Playwright.
 *
 * Each test:
 * 1. Runs a fixture JS file through the nxjs-wasm-test binary → JSON
 * 2. Runs the same fixture in a Chrome page via Playwright → JSON
 * 3. Compares the two JSON objects (deep equality)
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
const MODULES_DIR = join(ROOT, 'fixtures', 'modules');
const OUTPUT_DIR = join(ROOT, 'output');
const BUILD_DIR = join(ROOT, 'build');
const BINARY = join(BUILD_DIR, 'nxjs-wasm-test');
const RUNTIME = join(ROOT, '../../packages/runtime/runtime.js');
const HELPERS = join(BUILD_DIR, 'test-helpers.js');

function ensureDir(dir: string) {
	if (!existsSync(dir)) {
		mkdirSync(dir, { recursive: true });
	}
}

/**
 * Run a fixture through the nxjs-wasm-test binary and return parsed JSON.
 */
function runWithNxjs(fixturePath: string, outputPath: string): unknown {
	execSync(
		`"${BINARY}" "${RUNTIME}" "${HELPERS}" "${fixturePath}" "${outputPath}" "${MODULES_DIR}"`,
		{ timeout: 10000, stdio: ['pipe', 'pipe', 'pipe'] }
	);
	const raw = readFileSync(outputPath, 'utf-8');
	return JSON.parse(raw);
}

/**
 * Run a fixture in Chrome via Playwright and return parsed JSON.
 *
 * The fixture code uses `loadWasm(filename)` and `__output(obj)`.
 * In Chrome, we provide these via the native WebAssembly + fetch API.
 */
async function runWithChrome(
	browser: Browser,
	fixtureCode: string
): Promise<unknown> {
	const page = await browser.newPage();

	try {
		// Serve .wasm modules via a minimal data: page and evaluate
		await page.setContent(`<!DOCTYPE html><html><body></body></html>`);

		// Build a map of module files as base64
		const moduleFiles = readdirSync(MODULES_DIR).filter((f) =>
			f.endsWith('.wasm')
		);
		const modulesBase64: Record<string, string> = {};
		for (const f of moduleFiles) {
			modulesBase64[f] = readFileSync(join(MODULES_DIR, f)).toString(
				'base64'
			);
		}

		// Inject the modules and run the fixture
		const result = await page.evaluate(
			({ fixtureCode, modulesBase64 }) => {
				// Provide loadWasm() that decodes from base64
				(globalThis as any).loadWasm = function (filename: string) {
					const b64 = (modulesBase64 as any)[filename];
					if (!b64) throw new Error('Module not found: ' + filename);
					const binary = atob(b64);
					const bytes = new Uint8Array(binary.length);
					for (let i = 0; i < binary.length; i++) {
						bytes[i] = binary.charCodeAt(i);
					}
					return bytes.buffer;
				};

				// Capture __output result
				let outputResult: unknown = undefined;
				(globalThis as any).__output = function (val: unknown) {
					outputResult = val;
				};

				// Run the fixture (uses real WebAssembly built into Chrome)
				const fn = new Function(fixtureCode);
				fn();

				return outputResult;
			},
			{ fixtureCode, modulesBase64 }
		);

		return result;
	} finally {
		await page.close();
	}
}

// ---- Test Suite ----

describe('WebAssembly conformance: nx.js vs Chrome', () => {
	let browser: Browser;

	beforeAll(async () => {
		ensureDir(OUTPUT_DIR);

		if (!existsSync(BINARY)) {
			throw new Error(
				`WASM test binary not found at ${BINARY}. Run 'cmake -B build && cmake --build build' first.`
			);
		}
		if (!existsSync(RUNTIME)) {
			throw new Error(
				`Runtime not found at ${RUNTIME}. Run 'pnpm -w build:runtime' first.`
			);
		}
		if (!existsSync(HELPERS)) {
			throw new Error(
				`Test helpers not found at ${HELPERS}. Run 'cmake -B build && cmake --build build' first.`
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

		it(`${name}: matches Chrome`, async () => {
			const fixturePath = join(FIXTURES_DIR, fixture);
			const fixtureCode = readFileSync(fixturePath, 'utf-8');
			const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.json`);

			// Run with both engines
			const nxjsResult = runWithNxjs(fixturePath, nxjsOutputPath);
			const chromeResult = await runWithChrome(browser, fixtureCode);

			// Write Chrome output for debugging
			writeFileSync(
				join(OUTPUT_DIR, `chrome-${name}.json`),
				JSON.stringify(chromeResult, null, 2) + '\n'
			);

			// Log results
			console.log(`  ${name}: nx.js =`, JSON.stringify(nxjsResult));
			console.log(`  ${name}: Chrome =`, JSON.stringify(chromeResult));

			// Compare
			expect(nxjsResult).toEqual(chromeResult);
		});
	}
});
