import type { SwitchClass, WasmModuleOpaque } from './switch';
import { bufferSourceToArrayBuffer } from './utils';

declare const Switch: SwitchClass;

export interface GlobalDescriptor<T extends ValueType = ValueType> {
	mutable?: boolean;
	value: T;
}

export interface MemoryDescriptor {
	initial: number;
	maximum?: number;
	shared?: boolean;
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

export interface TableDescriptor {
	element: TableKind;
	initial: number;
	maximum?: number;
}

export interface ValueTypeMap {
	anyfunc: Function;
	externref: any;
	f32: number;
	f64: number;
	i32: number;
	i64: bigint;
	v128: never;
}

export interface WebAssemblyInstantiatedSource {
	instance: Instance;
	module: Module;
}

export type ImportExportKind = 'function' | 'global' | 'memory' | 'table';
export type TableKind = 'anyfunc' | 'externref';
export type ExportValue = Function | Global | Memory | Table;
export type Exports = Record<string, ExportValue>;
export type ImportValue = ExportValue | number;
export type Imports = Record<string, ModuleImports>;
export type ModuleImports = Record<string, ImportValue>;
export type ValueType = keyof ValueTypeMap;

export class CompileError extends Error implements WebAssembly.CompileError {
	name = 'CompileError';
}
export class RuntimeError extends Error implements WebAssembly.RuntimeError {
	name = 'RuntimeError';
}
export class LinkError extends Error implements WebAssembly.LinkError {
	name = 'LinkError';
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Global) */
export class Global<T extends ValueType = ValueType>
	implements WebAssembly.Global
{
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Global/value) */
	value: ValueTypeMap[T];

	constructor(descriptor: GlobalDescriptor<T>, v?: ValueTypeMap[T]) {
		throw new Error('Method not implemented.');
	}

	valueOf() {
		return this.value;
	}
}

export class Instance implements WebAssembly.Instance {
	readonly exports: Exports;

	constructor(module: Module, importObject?: Imports) {
		this.exports = {};
	}
}

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory)
 */
export class Memory implements WebAssembly.Memory {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/buffer) */
	readonly buffer: ArrayBuffer;

	constructor(descriptor: MemoryDescriptor) {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/grow) */
	grow(delta: number): number {
		throw new Error('Method not implemented.');
	}
}

interface ModuleInternals {
	buffer: ArrayBuffer;
	opaque: WasmModuleOpaque;
}

const moduleInternalsMap = new WeakMap<Module, ModuleInternals>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module) */
export class Module implements WebAssembly.Module {
	constructor(bytes: BufferSource) {
		const buffer = bufferSourceToArrayBuffer(bytes);
		moduleInternalsMap.set(this, {
			buffer,
			opaque: Switch.native.wasmNewModule(buffer),
		});
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/customSections) */
	static customSections(
		moduleObject: Module,
		sectionName: string
	): ArrayBuffer[] {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/exports) */
	static exports(moduleObject: Module): ModuleExportDescriptor[] {
		const i = moduleInternalsMap.get(moduleObject);
		if (!i) throw new Error(`No internal state for Module`);
		return Switch.native.wasmModuleExports(i.opaque);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/imports) */
	static imports(moduleObject: Module): ModuleImportDescriptor[] {
		const i = moduleInternalsMap.get(moduleObject);
		if (!i) throw new Error(`No internal state for Module`);
		return Switch.native.wasmModuleImports(i.opaque);
	}
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table) */
export class Table implements WebAssembly.Table {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/length) */
	readonly length: number;

	constructor(descriptor: TableDescriptor, value?: any) {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/get) */
	get(index: number) {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/grow) */
	grow(delta: number, value?: any): number {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/set) */
	set(index: number, value?: any): void {
		throw new Error('Method not implemented.');
	}
}

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/compile)
 */
export async function compile(bytes: BufferSource): Promise<Module> {
	return new Module(bytes);
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/compileStreaming) */
export async function compileStreaming(
	source: Response | PromiseLike<Response>
): Promise<Module> {
	const res = await source;
	if (!res.ok) {
		// TODO: throw error?
	}
	const buf = await res.arrayBuffer();
	return compile(buf);
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/instantiate) */
export function instantiate(
	bytes: BufferSource,
	importObject?: Imports
): Promise<WebAssemblyInstantiatedSource>;
export function instantiate(
	moduleObject: Module,
	importObject?: Imports
): Promise<Instance>;
export async function instantiate(
	bytes: BufferSource | Module,
	importObject?: Imports
) {
	if (bytes instanceof Module) {
		return new Instance(bytes, importObject);
	}
	const m = await compile(bytes);
	const instance = new Instance(m, importObject);
	return { module: m, instance };
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/instantiateStreaming) */
export async function instantiateStreaming(
	source: Response | PromiseLike<Response>,
	importObject?: Imports
): Promise<WebAssemblyInstantiatedSource> {
	const m = await compileStreaming(source);
	const instance = await instantiate(m, importObject);
	return { module: m, instance };
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/validate) */
export function validate(bytes: BufferSource): boolean {
	throw new Error('Method not implemented.');
}
