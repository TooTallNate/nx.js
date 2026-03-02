/**
 * Conformance Tests — nxjs-test vs Chrome
 *
 * Discovers bundled fixture .js files, runs each through:
 * 1. The nxjs-test binary (captures TAP from stdout)
 * 2. Chrome via Playwright (captures TAP from console.log)
 *
 * Parses both TAP outputs and compares assertion-by-assertion.
 */

import { execSync } from 'node:child_process';
import { existsSync, readdirSync, readFileSync } from 'node:fs';
import { basename, join } from 'node:path';
import { type Browser, chromium } from 'playwright';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { parseTap } from './src/tap-parser';

const ROOT = import.meta.dirname;
const BUILD_DIR = join(ROOT, 'build');
const FIXTURES_BUILD_DIR = join(ROOT, 'fixtures', 'build');
const BINARY = join(BUILD_DIR, 'nxjs-test');
const RUNTIME = join(ROOT, '../runtime.js');

/**
 * Run a bundled fixture through nxjs-test and return the TAP output.
 */
function runWithNxjs(fixturePath: string): string {
	try {
		const output = execSync(`"${BINARY}" "${RUNTIME}" "${fixturePath}"`, {
			timeout: 30_000,
			encoding: 'utf-8',
			stdio: ['pipe', 'pipe', 'pipe'],
		});
		return output;
	} catch (err: any) {
		// execSync throws on non-zero exit. The stdout may still have TAP output.
		if (err.stdout) return err.stdout;
		throw err;
	}
}

/**
 * Run a bundled fixture in Chrome via Playwright and return the TAP output.
 */
async function runWithChrome(
	browser: Browser,
	fixtureCode: string,
): Promise<string> {
	const page = await browser.newPage();
	const tapLines: string[] = [];

	try {
		// Capture console.log output as TAP lines
		page.on('console', (msg) => {
			if (msg.type() === 'log') {
				tapLines.push(msg.text());
			}
		});

		// Use route interception for secure context (needed by crypto.subtle)
		await page.route('**/*', (route) => {
			route.fulfill({
				status: 200,
				contentType: 'text/html',
				body: '<!DOCTYPE html><html><body></body></html>',
			});
		});
		await page.goto('https://localhost/test');

		// Evaluate the fixture as a module (esbuild outputs ESM format)
		await page.addScriptTag({ content: fixtureCode, type: 'module' });

		// Wait for TAP output to complete (look for the plan line 1..N)
		await page.waitForFunction(
			() => {
				// The tap library sets this when done
				return (globalThis as any).__tapDone === true;
			},
			{ timeout: 30_000 },
		);

		// Give a small delay for any remaining console.log to flush
		await page.waitForTimeout(100);

		return tapLines.join('\n');
	} finally {
		await page.close();
	}
}

// ---- Test Suite ----

describe('nxjs-test conformance: nx.js vs Chrome', () => {
	let browser: Browser;

	beforeAll(async () => {
		if (!existsSync(BINARY)) {
			throw new Error(
				`nxjs-test binary not found at ${BINARY}. ` +
					`Run 'cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build' in packages/runtime/test/`,
			);
		}
		if (!existsSync(RUNTIME)) {
			throw new Error(
				`runtime.js not found at ${RUNTIME}. ` +
					`Run 'pnpm -w build:runtime' first.`,
			);
		}
		if (!existsSync(FIXTURES_BUILD_DIR)) {
			throw new Error(
				`Built fixtures not found at ${FIXTURES_BUILD_DIR}. ` +
					`Run 'node test/build-fixtures.mjs' first.`,
			);
		}

		browser = await chromium.launch({
			args: ['--disable-gpu', '--no-sandbox'],
		});
	});

	afterAll(async () => {
		await browser?.close();
	});

	// Discover built fixtures
	const fixtures = existsSync(FIXTURES_BUILD_DIR)
		? readdirSync(FIXTURES_BUILD_DIR)
				.filter((f) => f.endsWith('.js'))
				.sort()
		: [];

	for (const fixture of fixtures) {
		const name = basename(fixture, '.js');

		it(`${name}: nxjs-test vs Chrome`, async () => {
			const fixturePath = join(FIXTURES_BUILD_DIR, fixture);
			const fixtureCode = readFileSync(fixturePath, 'utf-8');

			// Run in both engines
			const nxjsTap = runWithNxjs(fixturePath);
			const chromeTap = await runWithChrome(browser, fixtureCode);

			// Parse TAP output
			const nxjsResult = parseTap(nxjsTap);
			const chromeResult = parseTap(chromeTap);

			// Both should have produced assertions
			expect(nxjsResult.assertions.length).toBeGreaterThan(0);
			expect(chromeResult.assertions.length).toBeGreaterThan(0);

			// Same number of assertions
			expect(nxjsResult.assertions.length).toBe(chromeResult.assertions.length);

			// Compare each assertion
			for (let i = 0; i < nxjsResult.assertions.length; i++) {
				const nxjs = nxjsResult.assertions[i];
				const chrome = chromeResult.assertions[i];

				// Same test name
				expect(nxjs.name).toBe(chrome.name);

				// Same result — if they differ, show which engine failed
				if (nxjs.ok !== chrome.ok) {
					const failedIn = nxjs.ok ? 'Chrome' : 'nxjs-test';
					const passedIn = nxjs.ok ? 'nxjs-test' : 'Chrome';
					expect.fail(
						`Assertion #${nxjs.number} "${nxjs.name}": ` +
							`passed in ${passedIn} but failed in ${failedIn}`,
					);
				}
			}

			// Both engines should have no failures
			expect(nxjsResult.summary.fail).toBe(0);
			expect(chromeResult.summary.fail).toBe(0);
		});
	}
});
