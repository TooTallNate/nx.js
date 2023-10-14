import { build } from 'esbuild';

await build({
	bundle: true,
	minify: process.env.MINIFY === '1',
	mainFields: ['module', 'main'],
	//conditions: ['nxjs', 'import', 'browser', 'require', 'default'],
	target: 'es2020',
	entryPoints: ['src/index.ts'],
	sourcemap: true,
	sourcesContent: false,
	outfile: 'runtime.js',
});
