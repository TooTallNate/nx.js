import { bufferSourceToArrayBuffer } from './utils';
import type {
	SwitchClass,
	WasmModuleOpaque,
	WasmInstanceOpaque,
	WasmGlobalOpaque,
} from './switch';

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
		return i.opaque ? Switch.native.wasmGlobalGet(i.opaque) : i.value;
	}

	set value(v: ValueTypeMap[T]) {
		const i = globalInternalsMap.get(this)!;
		if (i.opaque) {
			Switch.native.wasmGlobalSet(i.opaque, v);
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

function bindGlobal(g: Global, opaque = Switch.native.wasmNewGlobal()) {
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
			} else {
				// TODO: Handle "memory" / "table" types
				throw new Error(`Unsupported import type`);
			}
			return {
				module: m,
				name: n,
				kind,
				val,
				i,
			};
		})
	);
}

function wrapExports(op: WasmInstanceOpaque, ex: any[]): Exports {
	const e: Exports = Object.create(null);
	for (const v of ex) {
		if (v.kind === 'function') {
			e[v.name] = callFunc.bind(null, op, v.name);
		} else if (v.kind === 'global') {
			const g = new Global({ value: v.value, mutable: v.mutable });
			bindGlobal(g, v.val);
			e[v.name] = g;
		} else if (v.kind === 'memory') {
			const m = Object.create(Memory.prototype);
			m.buffer = v.val;
			e[v.name] = m;
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
		const [opaque, exp] = Switch.native.wasmNewInstance(
			modInternal.opaque,
			unwrapImports(importObject)
		);
		instanceInternalsMap.set(this, { module: moduleObject, opaque });
		this.exports = wrapExports(opaque, exp);
	}
}

function callFunc(
	op: WasmInstanceOpaque,
	name: string,
	...args: unknown[]
): unknown {
	try {
		return Switch.native.wasmCallFunc(op, name, ...args);
	} catch (err: unknown) {
		throw toWasmError(err);
	}
}

const BYTES_PER_PAGE = 65536;

interface MemoryInternals {
	descriptor: MemoryDescriptor;
}

const memoryInternalsMap = new WeakMap<Memory, MemoryInternals>();

/**
 * [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory)
 */
export class Memory implements WebAssembly.Memory {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Memory/buffer) */
	readonly buffer: ArrayBuffer;

	constructor(descriptor: MemoryDescriptor) {
		const bytes = descriptor.initial * BYTES_PER_PAGE;
		this.buffer = descriptor.shared
			? new SharedArrayBuffer(bytes)
			: new ArrayBuffer(bytes);
		memoryInternalsMap.set(this, { descriptor });
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
			// Hold a reference to the bytes to prevent garbage collection
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
