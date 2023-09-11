import { build } from 'esbuild';

await build({
	bundle: true,
	minify: process.env.MINIFY === '1',
	mainFields: ['module', 'main'],
	target: 'es2020',
	entryPoints: ['src/index.ts'],
	outfile: 'runtime.js',
});
