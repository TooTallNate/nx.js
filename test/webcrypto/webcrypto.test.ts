/**
 * WebCrypto Conformance Tests
 *
 * Compares the nx.js WebCrypto implementation (compiled for host via the C
 * test binary) against Chrome's native WebCrypto via Playwright.
 *
 * Each test:
 * 1. Runs a fixture JS file through the nxjs-crypto-test binary → JSON
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

const ROOT = import.meta.dirname;
const FIXTURES_DIR = join(ROOT, 'fixtures');
const OUTPUT_DIR = join(ROOT, 'output');
const BUILD_DIR = join(ROOT, 'build');
const BINARY = join(BUILD_DIR, 'nxjs-crypto-test');
const RUNTIME = join(ROOT, '../../packages/runtime/runtime.js');
const HELPERS = join(BUILD_DIR, 'test-helpers.js');

function ensureDir(dir: string) {
	if (!existsSync(dir)) {
		mkdirSync(dir, { recursive: true });
	}
}

/**
 * Run a fixture through the nxjs-crypto-test binary and return parsed JSON.
 */
function runWithNxjs(fixturePath: string, outputPath: string): unknown {
	execSync(
		`"${BINARY}" "${RUNTIME}" "${HELPERS}" "${fixturePath}" "${outputPath}"`,
		{ timeout: 15000, stdio: ['pipe', 'pipe', 'pipe'] }
	);
	const raw = readFileSync(outputPath, 'utf-8');
	return JSON.parse(raw);
}

/**
 * Run a fixture in Chrome via Playwright and return parsed JSON.
 */
async function runWithChrome(
	browser: Browser,
	fixtureCode: string,
	helpersCode: string
): Promise<unknown> {
	const page = await browser.newPage();

	try {
		// crypto.subtle requires a secure context — use route interception
		// to serve a fake HTTPS page so SubtleCrypto is available.
		await page.route('**/*', (route) => {
			route.fulfill({
				status: 200,
				contentType: 'text/html',
				body: '<!DOCTYPE html><html><body></body></html>',
			});
		});
		await page.goto('https://localhost/test');

		// Set up __output capture
		await page.evaluate(() => {
			(globalThis as any).__outputResult = undefined;
			(globalThis as any).__output = function (val: unknown) {
				(globalThis as any).__outputResult = val;
			};
		});

		// Add helpers and fixture as script tags (preserves async behavior)
		await page.addScriptTag({ content: helpersCode });
		await page.addScriptTag({ content: fixtureCode });

		// Wait for __output to be called
		await page.waitForFunction(
			() => (globalThis as any).__outputResult !== undefined,
			{ timeout: 10000 }
		);

		return await page.evaluate(() => (globalThis as any).__outputResult);
	} finally {
		await page.close();
	}
}

// ---- Test Suite ----

describe('WebCrypto conformance: nx.js vs Chrome', () => {
	let browser: Browser;
	let helpersCode: string;

	beforeAll(async () => {
		ensureDir(OUTPUT_DIR);

		if (!existsSync(BINARY)) {
			throw new Error(
				`Crypto test binary not found at ${BINARY}. Run 'cmake -B build && cmake --build build' first.`
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

		helpersCode = readFileSync(HELPERS, 'utf-8');
		browser = await chromium.launch({ args: ['--disable-gpu', '--no-sandbox'] });
	});

	afterAll(async () => {
		await browser?.close();
	});

	const fixtures = readdirSync(FIXTURES_DIR)
		.filter((f) => f.endsWith('.js'))
		.sort();

	for (const fixture of fixtures) {
		const name = basename(fixture, '.js');

		it(`${name}: matches Chrome`, async () => {
			const fixturePath = join(FIXTURES_DIR, fixture);
			const fixtureCode = readFileSync(fixturePath, 'utf-8');
			const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.json`);

			const nxjsResult = runWithNxjs(fixturePath, nxjsOutputPath);
			const chromeResult = await runWithChrome(
				browser,
				fixtureCode,
				helpersCode
			);

			writeFileSync(
				join(OUTPUT_DIR, `chrome-${name}.json`),
				JSON.stringify(chromeResult, null, 2) + '\n'
			);

			console.log(`  ${name}: nx.js =`, JSON.stringify(nxjsResult));
			console.log(`  ${name}: Chrome =`, JSON.stringify(chromeResult));

			expect(nxjsResult).toEqual(chromeResult);
		});
	}
});
