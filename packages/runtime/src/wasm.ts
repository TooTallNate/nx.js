import { $ } from './$';
import { bufferSourceToArrayBuffer, proto } from './utils';
import type { BufferSource } from './types';
import type {
	WasmModuleOpaque,
	WasmInstanceOpaque,
	WasmGlobalOpaque,
} from './internal';

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

function toWasmError(e: unknown) {
	if (e && e instanceof Error && 'wasmError' in e) {
		switch (e.wasmError) {
			case 'CompileError':
				return new CompileError(e.message);
			case 'LinkError':
				return new LinkError(e.message);
			case 'RuntimeError':
				return new RuntimeError(e.message);
		}
	}
	return e;
}

interface GlobalInternals<T extends ValueType = ValueType> {
	descriptor: GlobalDescriptor<T>;
	value?: ValueTypeMap[T];
	opaque?: WasmGlobalOpaque;
}

const globalInternalsMap = new WeakMap<Global, GlobalInternals<any>>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Global) */
export class Global<T extends ValueType = ValueType>
	implements WebAssembly.Global
{
	constructor(descriptor: GlobalDescriptor<T>, value?: ValueTypeMap[T]) {
		globalInternalsMap.set(this, { descriptor, value });
	}

	/**
	 * The value contained inside the global variable — this can be used to directly set and get the global's value.
	 */
	get value(): ValueTypeMap[T] {
		const i = globalInternalsMap.get(this)!;
		return i.opaque ? $.wasmGlobalGet(i.opaque) : i.value;
	}

	set value(v: ValueTypeMap[T]) {
		const i = globalInternalsMap.get(this)!;
		if (i.opaque) {
			$.wasmGlobalSet(i.opaque, v);
		} else {
			i.value = v;
		}
	}

	/**
	 * Old-style method that returns the value contained inside the global variable.
	 */
	valueOf() {
		return this.value;
	}
}

function bindGlobal(g: Global, opaque = $.wasmNewGlobal()) {
	const i = globalInternalsMap.get(g);
	if (!i) throw new Error(`No internal state for Global`);
	i.opaque = opaque;
	return opaque;
}

function unwrapImports(importObject: Imports = {}) {
	return Object.entries(importObject).flatMap(([m, i]) =>
		Object.entries(i).map(([n, v]) => {
			let val;
			let i;
			let kind: ImportExportKind;
			if (typeof v === 'function') {
				kind = 'function';
				val = v;
			} else if (v instanceof Global) {
				kind = 'global';
				i = v.value;
				val = bindGlobal(v);
			} else if (v instanceof Memory) {
				kind = 'memory';
				val = v;
			} else {
				// TODO: Handle "table" type
				throw new LinkError(`Unsupported import type for "${m}.${n}"`);
			}
			return {
				module: m,
				name: n,
				kind,
				val,
				i,
			};
		}),
	);
}

function wrapExports(ex: any[]): Exports {
	const e: Exports = Object.create(null);
	for (const v of ex) {
		if (v.kind === 'function') {
			const fn = callFunc.bind(null, v.val);
			Object.defineProperty(fn, 'name', { value: v.name });
			e[v.name] = fn;
		} else if (v.kind === 'global') {
			const g = new Global({ value: v.value, mutable: v.mutable });
			bindGlobal(g, v.val);
			e[v.name] = g;
		} else if (v.kind === 'memory') {
			e[v.name] = proto(v.val, Memory);
		} else if (v.kind === 'table') {
			e[v.name] = proto(v.val, Table);
		} else {
			throw new LinkError(`Unsupported export type "${v.kind}"`);
		}
	}
	return Object.freeze(e);
}

interface InstanceInternals {
	module: Module;
	opaque: WasmInstanceOpaque;
}

const instanceInternalsMap = new WeakMap<Instance, InstanceInternals>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Instance) */
export class Instance implements WebAssembly.Instance {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Instance/exports) */
	readonly exports: Exports;

	constructor(moduleObject: Module, importObject?: Imports) {
		const modInternal = moduleInternalsMap.get(moduleObject);
		if (!modInternal) throw new Error(`No internal state for Module`);
		const [opaque, exp] = $.wasmNewInstance(
			modInternal.opaque,
			unwrapImports(importObject),
		);
		instanceInternalsMap.set(this, { module: moduleObject, opaque });
		this.exports = wrapExports(exp);
	}
}

function callFunc(
	func: any, // exported func
	...args: unknown[]
): unknown {
	try {
		return $.wasmCallFunc(func, ...args);
	} catch (err: unknown) {
		throw toWasmError(err);
	}
}

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory)
 */
export class Memory implements WebAssembly.Memory {
	constructor(descriptor: MemoryDescriptor) {
		return proto($.wasmMemNew(descriptor), Memory);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/buffer) */
	declare readonly buffer: ArrayBuffer;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/grow) */
	// @ts-expect-error This is a native function
	grow(delta: number): number {}
}
$.wasmInitMemory(Memory);

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
			// Hold a reference to the bytes to prevent garbage collection
			buffer,
			opaque: $.wasmNewModule(buffer),
		});
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/customSections) */
	static customSections(
		moduleObject: Module,
		sectionName: string,
	): ArrayBuffer[] {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/exports) */
	static exports(moduleObject: Module): ModuleExportDescriptor[] {
		const i = moduleInternalsMap.get(moduleObject);
		if (!i) throw new Error(`No internal state for Module`);
		return $.wasmModuleExports(i.opaque);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/imports) */
	static imports(moduleObject: Module): ModuleImportDescriptor[] {
		const i = moduleInternalsMap.get(moduleObject);
		if (!i) throw new Error(`No internal state for Module`);
		return $.wasmModuleImports(i.opaque);
	}
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table) */
export class Table implements WebAssembly.Table {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/length) */
	declare readonly length: number;

	constructor(descriptor: TableDescriptor, value?: any) {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Table/get) */
	get(index: number): any {
		// Only function refs are supported for now
		const fn = $.wasmTableGet(this, index);
		return callFunc.bind(null, fn);
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
$.wasmInitTable(Table);

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/compile)
 */
export async function compile(bytes: BufferSource): Promise<Module> {
	// TODO: run this on the thread pool?
	return new Module(bytes);
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/compileStreaming) */
export async function compileStreaming(
	source: Response | PromiseLike<Response>,
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
	importObject?: Imports,
): Promise<WebAssemblyInstantiatedSource>;
export function instantiate(
	moduleObject: Module,
	importObject?: Imports,
): Promise<Instance>;
export async function instantiate(
	bytes: BufferSource | Module,
	importObject?: Imports,
) {
	if (bytes instanceof Module) {
		return new Instance(bytes, importObject);
	}
	const m = await compile(bytes);

	// TODO: run this on the thread pool?
	// maybe not, because we need to interact with JS here?
	const instance = new Instance(m, importObject);

	return { module: m, instance };
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/instantiateStreaming) */
export async function instantiateStreaming(
	source: Response | PromiseLike<Response>,
	importObject?: Imports,
): Promise<WebAssemblyInstantiatedSource> {
	const m = await compileStreaming(source);
	const instance = await instantiate(m, importObject);
	return { module: m, instance };
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/validate) */
export function validate(bytes: BufferSource): boolean {
	throw new Error('Method not implemented.');
}
