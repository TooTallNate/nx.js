/**
 * Canvas 2D Conformance Tests
 *
 * Compares the nx.js Canvas 2D implementation (compiled for host via the C test
 * binary) against Chrome's Canvas 2D rendering via Playwright.
 *
 * Each test:
 * 1. Runs a fixture JS file through the nxjs-canvas-test binary → PNG
 * 2. Runs the same fixture in a Chrome <canvas> via Playwright → PNG
 * 3. Compares the two PNGs using pixelmatch
 * 4. Asserts pixel difference is below threshold
 */

import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import { execSync } from 'node:child_process';
import { readFileSync, readdirSync, mkdirSync, existsSync, writeFileSync } from 'node:fs';
import { join, basename } from 'node:path';
import { chromium, type Browser } from 'playwright';
import pixelmatch from 'pixelmatch';
import { PNG } from 'pngjs';

const CANVAS_WIDTH = 200;
const CANVAS_HEIGHT = 200;

// Directories
const ROOT = import.meta.dirname;
const FIXTURES_DIR = join(ROOT, 'fixtures');
const OUTPUT_DIR = join(ROOT, 'output');
const BUILD_DIR = join(ROOT, 'build');
const BINARY = join(BUILD_DIR, 'nxjs-canvas-test');
const BRIDGE = join(BUILD_DIR, 'canvas_api.js');

// Pixel difference threshold: percentage of total pixels that can differ
// Cairo and Chrome use different rasterizers, so some anti-aliasing differences
// are expected. 5% is generous enough for AA differences but catches real bugs.
const DIFF_THRESHOLD_PERCENT = 0.05; // 5%

// pixelmatch color threshold (0-1): how different a pixel must be to count as
// "different". 0.1 is the default, which tolerates minor anti-aliasing.
const PIXEL_THRESHOLD = 0.1;

function ensureDir(dir: string) {
	if (!existsSync(dir)) {
		mkdirSync(dir, { recursive: true });
	}
}

/**
 * Run a fixture through the nx.js canvas test binary and return the PNG buffer.
 */
function renderWithNxjs(fixturePath: string, outputPath: string): Buffer {
	execSync(
		`"${BINARY}" "${BRIDGE}" "${fixturePath}" "${outputPath}" ${CANVAS_WIDTH} ${CANVAS_HEIGHT}`,
		{ timeout: 10000 }
	);
	return readFileSync(outputPath);
}

/**
 * Run a fixture in Chrome via Playwright and return the PNG buffer.
 */
async function renderWithChrome(
	browser: Browser,
	fixtureCode: string,
	outputPath: string
): Promise<Buffer> {
	const page = await browser.newPage({
		viewport: { width: CANVAS_WIDTH + 40, height: CANVAS_HEIGHT + 40 },
		deviceScaleFactor: 1,
	});

	try {
		// Create a minimal HTML page with a canvas
		await page.setContent(`
			<!DOCTYPE html>
			<html>
			<head>
				<style>
					* { margin: 0; padding: 0; }
					canvas { display: block; }
				</style>
			</head>
			<body>
				<canvas id="c" width="${CANVAS_WIDTH}" height="${CANVAS_HEIGHT}"></canvas>
				<script>
					function createCanvas(w, h) {
						var canvas = document.getElementById('c');
						if (w !== undefined) canvas.width = w;
						if (h !== undefined) canvas.height = h;
						var ctx = canvas.getContext('2d');
						return { canvas: canvas, ctx: ctx };
					}
				</script>
			</body>
			</html>
		`);

		// Run the fixture code
		await page.evaluate(fixtureCode);

		// Get the canvas as PNG via data URL
		const dataUrl = await page.evaluate(() => {
			return document.getElementById('c')!.toDataURL('image/png');
		}) as string;

		// Convert data URL to buffer
		const base64 = dataUrl.replace(/^data:image\/png;base64,/, '');
		const buffer = Buffer.from(base64, 'base64');
		writeFileSync(outputPath, buffer);
		return buffer;
	} finally {
		await page.close();
	}
}

/**
 * Compare two PNG buffers and return the diff percentage.
 */
function compareImages(
	nxjsPng: Buffer,
	chromePng: Buffer,
	diffOutputPath: string
): { diffPercent: number; numDiffPixels: number; totalPixels: number } {
	const img1 = PNG.sync.read(nxjsPng);
	const img2 = PNG.sync.read(chromePng);

	const width = Math.max(img1.width, img2.width);
	const height = Math.max(img1.height, img2.height);
	const totalPixels = width * height;

	const diff = new PNG({ width, height });

	const numDiffPixels = pixelmatch(
		img1.data,
		img2.data,
		diff.data,
		width,
		height,
		{ threshold: PIXEL_THRESHOLD }
	);

	// Write diff image for debugging
	writeFileSync(diffOutputPath, PNG.sync.write(diff));

	return {
		diffPercent: numDiffPixels / totalPixels,
		numDiffPixels,
		totalPixels,
	};
}

// ---- Test Suite ----

describe('Canvas 2D conformance: nx.js vs Chrome', () => {
	let browser: Browser;

	beforeAll(async () => {
		ensureDir(OUTPUT_DIR);

		// Verify the binary exists
		if (!existsSync(BINARY)) {
			throw new Error(
				`Canvas test binary not found at ${BINARY}. Run 'pnpm build' first.`
			);
		}
		if (!existsSync(BRIDGE)) {
			throw new Error(
				`Canvas API bridge not found at ${BRIDGE}. Run 'pnpm build' first.`
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

		it(`${name}: renders matching Chrome`, async () => {
			const fixturePath = join(FIXTURES_DIR, fixture);
			const fixtureCode = readFileSync(fixturePath, 'utf-8');

			const nxjsOutputPath = join(OUTPUT_DIR, `nxjs-${name}.png`);
			const chromeOutputPath = join(OUTPUT_DIR, `chrome-${name}.png`);
			const diffOutputPath = join(OUTPUT_DIR, `diff-${name}.png`);

			// Render with both engines
			const nxjsPng = renderWithNxjs(fixturePath, nxjsOutputPath);
			const chromePng = await renderWithChrome(
				browser,
				fixtureCode,
				chromeOutputPath
			);

			// Compare
			const { diffPercent, numDiffPixels, totalPixels } = compareImages(
				nxjsPng,
				chromePng,
				diffOutputPath
			);

			// Log results for debugging
			console.log(
				`  ${name}: ${numDiffPixels}/${totalPixels} pixels differ (${(diffPercent * 100).toFixed(2)}%)`
			);

			expect(diffPercent).toBeLessThan(DIFF_THRESHOLD_PERCENT);
		});
	}
});
