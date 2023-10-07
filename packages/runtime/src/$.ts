import type { MemoryDescriptor, Memory } from './wasm';

export interface Init {
	wasmMemNew(descriptor: MemoryDescriptor): Memory;
	wasmMemProto(mem: any): void;
}

export const $: Init = (globalThis as any).$;
delete (globalThis as any).$;
