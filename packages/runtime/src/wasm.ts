import type { Switch as _Switch } from './switch';
declare const Switch: _Switch;

export class Instance implements WebAssembly.Instance {
	exports: WebAssembly.Exports;
	constructor() {
		this.exports = {};
	}
}

export class Module implements WebAssembly.Module {
	static customSections(
		moduleObject: Module,
		sectionName: string
	): ArrayBuffer[] {
		throw new Error('Method not implemented.');
	}
	static exports(moduleObject: Module): WebAssembly.ModuleExportDescriptor[] {
		throw new Error('Method not implemented.');
	}
	static imports(moduleObject: Module): WebAssembly.ModuleImportDescriptor[] {
		throw new Error('Method not implemented.');
	}
}

export const compile: (typeof WebAssembly)['compile'] = async (bytes) => {
	const buffer =
		bytes instanceof ArrayBuffer
			? bytes
			: bytes.buffer.slice(
					bytes.byteOffset,
					bytes.byteOffset + bytes.byteLength
			  );
	return new Module();
};

export function instantiate(
	bytes: BufferSource,
	importObject?: WebAssembly.Imports
): Promise<WebAssembly.WebAssemblyInstantiatedSource>;
export function instantiate(
	moduleObject: Module,
	importObject?: WebAssembly.Imports
): Promise<WebAssembly.Instance>;
export async function instantiate(
	bytes: BufferSource | Module,
	importObject?: WebAssembly.Imports
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
