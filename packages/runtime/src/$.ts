import type { Callback, NetworkInfo } from './types';
import type { Server } from './tcp';
import type { MemoryDescriptor, Memory } from './wasm';
import type { VirtualKeyboard } from './navigator/virtual-keyboard';

export interface Init {
	// battery.c
	batteryInit(): void;
	batteryInitClass(c: any): void;
	batteryExit(): void;

	// error.c
	onError(fn: (err: any) => number): void;
	onUnhandledRejection(
		fn: (promise: Promise<unknown>, reason: any) => number
	): void;

	// nifm.c
	nifmInitialize(): () => void;
	networkInfo(): NetworkInfo;

	// swkbd.c
	swkbdCreate(fns: {
		onCancel: () => void;
		onChange: (
			str: string,
			cursorPos: number,
			dicStartCursorPos: number,
			dicEndCursorPos: number
		) => void;
		onSubmit: (str: string) => void;
		onCursorMove: (str: string, cursorPos: number) => void;
	}): VirtualKeyboard;
	swkbdShow(s: VirtualKeyboard): [number, number, number, number];
	swkbdHide(s: VirtualKeyboard): void;
	swkbdExit(this: VirtualKeyboard): void;
	swkbdUpdate(this: VirtualKeyboard): void;

	// tcp.c
	connect(cb: Callback<number>, ip: string, port: number): void;
	write(cb: Callback<number>, fd: number, data: ArrayBuffer): void;
	read(cb: Callback<number>, fd: number, buffer: ArrayBuffer): void;
	tcpInitServer(c: any): void;
	tcpServerNew(
		ip: string,
		port: number,
		onAccept: (fd: number) => void
	): Server;

	// wasm.c
	wasmCallFunc(f: any, ...args: unknown[]): unknown;
	wasmMemNew(descriptor: MemoryDescriptor): Memory;
	wasmTableGet(t: any, i: number): Memory;
	wasmInitMemory(c: any): void;
	wasmInitTable(c: any): void;
}

export const $: Init = (globalThis as any).$;
delete (globalThis as any).$;
