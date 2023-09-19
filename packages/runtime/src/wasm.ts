import type { SwitchClass } from './switch';

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

export class CompileError extends Error implements WebAssembly.CompileError {}
export class RuntimeError extends Error implements WebAssembly.RuntimeError {}
export class LinkError extends Error implements WebAssembly.LinkError {}

export class Global<T extends ValueType = ValueType>
	implements WebAssembly.Global
{
	value: any;

	constructor(descriptor: GlobalDescriptor<T>, v?: ValueTypeMap[T]) {
		throw new Error('Method not implemented.');
	}

	valueOf() {
		throw new Error('Method not implemented.');
	}
}

export class Instance implements WebAssembly.Instance {
	exports: Exports;
	constructor() {
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

export class Module implements WebAssembly.Module {
	constructor(bytes: BufferSource) {
		throw new Error('Method not implemented.');
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
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/WebAssembly/Module/imports) */
	static imports(moduleObject: Module): ModuleImportDescriptor[] {
		throw new Error('Method not implemented.');
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
	const buffer =
		bytes instanceof ArrayBuffer
			? bytes
			: bytes.buffer.slice(
					bytes.byteOffset,
					bytes.byteOffset + bytes.byteLength
			  );
	return new Module(buffer);
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
		return new Instance();
	}
	const module = await compile(bytes);
	return {
		instance: new Instance(),
		module,
	};
}
