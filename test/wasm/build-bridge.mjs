/**
 * Builds the WebAssembly JS bridge by bundling the real runtime wasm.ts
 * into a single IIFE that QuickJS can evaluate.
 *
 * This ensures we test the actual runtime JavaScript code — not a
 * hand-written reimplementation that can (and did) diverge.
 *
 * Usage: node build-bridge.mjs
 */

import { build } from 'esbuild';
import { readFile } from 'fs/promises';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

await build({
	entryPoints: [resolve(__dirname, 'src/bridge-entry.ts')],
	bundle: true,
	format: 'iife',
	target: 'es2020',
	outfile: resolve(__dirname, 'build/wasm_api.js'),
	treeShaking: true,
	logLevel: 'info',
	plugins: [
		{
			name: 'strip-iterator-helpers',
			setup(build) {
				// packages/runtime/src/utils.ts has a top-level block that
				// references Iterator.prototype and Iterator.from (TC39
				// Iterator Helpers). QuickJS may not support these yet.
				// Since wasm.ts doesn't use FunctionPrototypeWithIteratorHelpers,
				// we strip that block to keep the bundle QuickJS-compatible
				// while preserving the real proto() and bufferSourceToArrayBuffer().
				build.onLoad(
					{ filter: /packages[\\/]runtime[\\/]src[\\/]utils\.ts$/ },
					async (args) => {
						let contents = await readFile(args.path, 'utf8');
						// Strip from the FunctionPrototypeWithIteratorHelpers
						// declaration to end of file
						contents = contents.replace(
							/export const FunctionPrototypeWithIteratorHelpers[\s\S]*/,
							'// [stripped: FunctionPrototypeWithIteratorHelpers — not needed by wasm.ts]\n',
						);
						return { contents, loader: 'ts' };
					},
				);
			},
		},
	],
});
