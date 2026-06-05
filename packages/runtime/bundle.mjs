import { build } from 'esbuild';

await build({
	bundle: true,
	minify: process.env.MINIFY === '1',
	mainFields: ['module', 'main'],
	//conditions: ['nxjs', 'import', 'browser', 'require', 'default'],
	target: 'es2022',
	keepNames: true,
	// Inject a module-scoped `process` shim into any module that references
	// `process` as a free variable. Only `@xterm/headless` does, and only for
	// its eval-time `isNode` check — this stops xterm from reading
	// `navigator.userAgent` during runtime startup (which transitively touches
	// not-yet-ready globals and threw on device + host). See the shim file.
	inject: ['./xterm-process-shim.js'],
	entryPoints: ['src/index.ts'],
	sourcemap: true,
	sourcesContent: false,
	outfile: 'runtime.js',
});
