// WebAssembly is provided NATIVELY by V8 — there is no runtime shim here (the
// wasm3-backed `./wasm` module was removed in the V8 migration). This file
// exists ONLY to supply the `WebAssembly` namespace TypeScript types for the
// published `index.d.ts`, since the runtime's type bundle uses
// `no-default-lib` and therefore does not pull `WebAssembly` from `lib.dom`.
//
// `build.mjs` consumes the exports below and emits them as
// `declare namespace WebAssembly { ... }`. Keep these as type-only
// declarations that mirror the standard WebAssembly JS API.

import type { BufferSource } from './types';

export type ValueType =
	| 'i32'
	| 'i64'
	| 'f32'
	| 'f64'
	| 'v128'
	| 'externref'
	| 'anyfunc';

export type ImportExportKind = 'function' | 'global' | 'memory' | 'table';
export type TableKind = 'anyfunc' | 'externref';

export interface GlobalDescriptor<T extends ValueType = ValueType> {
	mutable?: boolean;
	value: T;
}

export interface MemoryDescriptor {
	initial: number;
	maximum?: number;
	shared?: boolean;
}

export interface TableDescriptor {
	element: TableKind;
	initial: number;
	maximum?: number;
}

export interface ModuleExportDescriptor {
	kind: ImportExportKind;
	name: string;
}

export interface ModuleImportDescriptor {
	kind: ImportExportKind;
	module: string;
	name: string;
}

export type ExportValue = Function | Global | Memory | Table;
export type Exports = Record<string, ExportValue>;
export type ImportValue = ExportValue | number;
export type ModuleImports = Record<string, ImportValue>;
export type Imports = Record<string, ModuleImports>;

export interface WebAssemblyInstantiatedSource {
	instance: Instance;
	module: Module;
}

export declare class CompileError extends Error {
	constructor(message?: string);
}

export declare class LinkError extends Error {
	constructor(message?: string);
}

export declare class RuntimeError extends Error {
	constructor(message?: string);
}

export declare class Global<T extends ValueType = ValueType> {
	value: any;
	constructor(descriptor: GlobalDescriptor<T>, v?: any);
	valueOf(): any;
}

export declare class Memory {
	readonly buffer: ArrayBuffer;
	constructor(descriptor: MemoryDescriptor);
	grow(delta: number): number;
}

export declare class Table {
	readonly length: number;
	constructor(descriptor: TableDescriptor, value?: any);
	get(index: number): any;
	grow(delta: number, value?: any): number;
	set(index: number, value?: any): void;
}

export declare class Module {
	constructor(bytes: BufferSource);
	static customSections(
		moduleObject: Module,
		sectionName: string,
	): ArrayBuffer[];
	static exports(moduleObject: Module): ModuleExportDescriptor[];
	static imports(moduleObject: Module): ModuleImportDescriptor[];
}

export declare class Instance {
	readonly exports: Exports;
	constructor(module: Module, importObject?: Imports);
}

export declare function compile(bytes: BufferSource): Promise<Module>;
export declare function compileStreaming(
	source: Response | PromiseLike<Response>,
): Promise<Module>;
export declare function instantiate(
	bytes: BufferSource,
	importObject?: Imports,
): Promise<WebAssemblyInstantiatedSource>;
export declare function instantiate(
	moduleObject: Module,
	importObject?: Imports,
): Promise<Instance>;
export declare function instantiateStreaming(
	source: Response | PromiseLike<Response>,
	importObject?: Imports,
): Promise<WebAssemblyInstantiatedSource>;
export declare function validate(bytes: BufferSource): boolean;
