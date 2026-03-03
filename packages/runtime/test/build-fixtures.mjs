/**
 * Bundles test fixture TypeScript files into self-contained JS files
 * that can be evaluated in both QuickJS (nxjs-test) and Chrome (Playwright).
 *
 * Each fixture imports from ../src/tap.ts. esbuild inlines the tap library
 * so the output is a single file with no external dependencies.
 */

import { existsSync, mkdirSync, readdirSync } from 'node:fs';
import { basename, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { build } from 'esbuild';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const FIXTURES_DIR = join(__dirname, 'fixtures');
const OUTPUT_DIR = join(FIXTURES_DIR, 'build');

// Ensure output directory exists
if (!existsSync(OUTPUT_DIR)) {
	mkdirSync(OUTPUT_DIR, { recursive: true });
}

// Find all .ts fixture files
const fixtures = readdirSync(FIXTURES_DIR)
	.filter((f) => f.endsWith('.ts'))
	.map((f) => join(FIXTURES_DIR, f));

if (fixtures.length === 0) {
	console.log('No fixture files found');
	process.exit(0);
}

console.log(`Bundling ${fixtures.length} fixtures...`);

await build({
	entryPoints: fixtures,
	bundle: true,
	format: 'esm',
	target: 'es2022',
	outdir: OUTPUT_DIR,
	outExtension: { '.js': '.js' },
	// No external dependencies — everything is inlined
	platform: 'neutral',
	loader: { '.wasm': 'binary' },
	// Suppress banner/footer for clean output
	logLevel: 'warning',
});

for (const f of fixtures) {
	console.log(`  ${basename(f, '.ts')}.js`);
}
console.log('Done.');
