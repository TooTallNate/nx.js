/**
 * Bridge entry point for the WebAssembly test harness.
 *
 * Instead of a hand-written reimplementation, this imports the REAL
 * wasm.ts from the nx.js runtime package so the test harness exercises
 * the actual JS code that runs on the Switch. Combined with the real
 * wasm.c (compiled for x86_64 via CMake), this means both the C and
 * JS layers are tested against Chrome's native WebAssembly.
 *
 * The entry is bundled by esbuild into a single IIFE that QuickJS
 * can evaluate (see build-bridge.mjs).
 */

import * as Wasm from '../../../packages/runtime/src/wasm';

// Assign the real WebAssembly namespace to the global scope.
// In the runtime this is done via def() in index.ts; here we just
// need the classes and functions to be reachable from test fixtures.
(globalThis as any).WebAssembly = {
	Module: Wasm.Module,
	Instance: Wasm.Instance,
	Memory: Wasm.Memory,
	Table: Wasm.Table,
	Global: Wasm.Global,
	compile: Wasm.compile,
	instantiate: Wasm.instantiate,
	compileStreaming: Wasm.compileStreaming,
	instantiateStreaming: Wasm.instantiateStreaming,
	validate: Wasm.validate,
	CompileError: Wasm.CompileError,
	RuntimeError: Wasm.RuntimeError,
	LinkError: Wasm.LinkError,
};

// Provide loadWasm() for test fixtures.
// readFile() and __modules_dir are injected by main.c.
declare const readFile: (path: string) => ArrayBuffer;
declare const __modules_dir: string;

(globalThis as any).loadWasm = function (filename: string) {
	return readFile(__modules_dir + '/' + filename);
};
