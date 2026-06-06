import { build } from 'esbuild';

await build({
	bundle: true,
	minify: process.env.MINIFY === '1',
	mainFields: ['module', 'main'],
	//conditions: ['nxjs', 'import', 'browser', 'require', 'default'],
	target: 'es2022',
	keepNames: true,
	// Inject a module-scoped `process` shim into every bundled module that
	// references `process` as a free variable. In this bundle that is both
	// `@xterm/headless` (its eval-time `isNode` check — the shim stops xterm
	// reading `navigator.userAgent` during startup, which touched not-yet-ready
	// globals and threw on device + host) AND `kleur/colors` (its eval-time
	// color auto-detection reads `process.env`/`process.stdout.isTTY`, so the
	// shim must keep colors enabled — see the shim file for details). It is NOT
	// a real global; `globalThis.process` stays undefined.
	inject: ['./xterm-process-shim.js'],
	entryPoints: ['src/index.ts'],
	sourcemap: true,
	sourcesContent: false,
	outfile: 'runtime.js',
});
