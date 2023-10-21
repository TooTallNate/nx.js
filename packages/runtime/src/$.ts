import type { MemoryDescriptor, Memory } from './wasm';

export interface Init {
	// battery.c
	batteryInit(): void;
	batteryInitClass(c: any): void;
	batteryExit(): void;

	// error.c
	onError(fn: (err: any) => void): void;
	onUnhandledRejection(
		fn: (promise: Promise<unknown>, reason: any) => void
	): void;

	// wasm.c
	wasmCallFunc(f: any, ...args: unknown[]): unknown;
	wasmMemNew(descriptor: MemoryDescriptor): Memory;
	wasmTableGet(t: any, i: number): Memory;
	wasmInitMemory(c: any): void;
	wasmInitTable(c: any): void;
}

export const $: Init = (globalThis as any).$;
delete (globalThis as any).$;
