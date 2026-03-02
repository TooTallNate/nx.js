/**
 * Bundles each test file into a single JS file that can be evaluated
 * by the QuickJS runtime test binary (no module resolution).
 */
import { build } from 'esbuild';
import { readdirSync, mkdirSync } from 'fs';
import { join, basename } from 'path';
import { fileURLToPath } from 'url';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const TESTS_DIR = join(__dirname, 'tests');
const OUT_DIR = join(__dirname, 'dist');

mkdirSync(OUT_DIR, { recursive: true });

const testFiles = readdirSync(TESTS_DIR).filter(f => f.endsWith('.ts'));

for (const file of testFiles) {
	await build({
		entryPoints: [join(TESTS_DIR, file)],
		bundle: true,
		format: 'iife',
		target: 'es2022',
		outfile: join(OUT_DIR, basename(file, '.ts') + '.js'),
		// QuickJS doesn't have process.env — define it away
		define: {
			'process.env.NXJS': '"0"',
		},
		// Don't bundle things that the runtime provides
		external: [],
	});
}

console.log(`Bundled ${testFiles.length} test files to ${OUT_DIR}`);
