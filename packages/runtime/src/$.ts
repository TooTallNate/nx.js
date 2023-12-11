import type { NetworkInfo } from './types';
import type { Callback, RGBA } from './internal';
import type { Server, TlsContextOpaque } from './tcp';
import type { MemoryDescriptor, Memory } from './wasm';
import type { BatteryManager } from './navigator/battery';
import type { VirtualKeyboard } from './navigator/virtual-keyboard';
import type { OffscreenCanvas } from './canvas/offscreen-canvas';
import type { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';
import type { OffscreenCanvasRenderingContext2D } from './canvas/offscreen-canvas-rendering-context-2d';
import type { Image } from './image';
import type { Screen } from './screen';
import type { FontFace } from './font/font-face';

type ClassOf<T> = {
	new (...args: any[]): T;
};

export interface Init {
	// battery.c
	batteryInit(): void;
	batteryInitClass(c: ClassOf<BatteryManager>): void;
	batteryExit(): void;

	// canvas.c
	canvasNew(width: number, height: number): Screen | OffscreenCanvas;
	canvasInitClass(c: ClassOf<Screen | OffscreenCanvas>): void;
	canvasContext2dNew(c: Screen): CanvasRenderingContext2D;
	canvasContext2dNew(c: OffscreenCanvas): OffscreenCanvasRenderingContext2D;
	canvasContext2dInitClass(
		c: ClassOf<CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D>
	): void;
	canvasContext2dGetImageData(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		sx: number,
		sy: number,
		sw: number,
		sh: number
	): ArrayBuffer;
	canvasContext2dGetTransform(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D
	): number[];
	canvasContext2dSetFont(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		font: FontFace,
		size: number
	): number[];
	canvasContext2dSetFillStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): number[];
	canvasContext2dSetStrokeStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): number[];

	// dns.c
	dnsResolve(cb: Callback<string[]>, hostname: string): void;

	// error.c
	onError(fn: (err: any) => number): void;
	onUnhandledRejection(
		fn: (promise: Promise<unknown>, reason: any) => number
	): void;

	// font.c
	fontFaceNew(data: ArrayBuffer): FontFace;
	getSystemFont(): ArrayBuffer;

	// image.c
	imageNew(): Image;
	imageDecode(
		cb: Callback<{
			width: number;
			height: number;
		}>,
		img: Image,
		data: ArrayBuffer
	): void;

	// main.c
	print(v: string): void;
	printErr(v: string): void;
	getInternalPromiseState(p: Promise<unknown>): [number, unknown];
	getenv(name: string): string | undefined;
	setenv(name: string, value: string): void;
	unsetenv(name: string): void;
	envToObject(): Record<string, string>;
	onFrame(fn: (kDown: number) => void): void;
	onExit(fn: () => void): void;
	framebufferInit(screen: Screen): void;

	// nifm.c
	nifmInitialize(): () => void;
	networkInfo(): NetworkInfo;

	// software-keyboard.c
	swkbdCreate(fns: {
		onCancel: (this: VirtualKeyboard) => void;
		onChange: (
			this: VirtualKeyboard,
			str: string,
			cursorPos: number,
			dicStartCursorPos: number,
			dicEndCursorPos: number
		) => void;
		onSubmit: (this: VirtualKeyboard, str: string) => void;
		onCursorMove: (
			this: VirtualKeyboard,
			str: string,
			cursorPos: number
		) => void;
	}): VirtualKeyboard;
	swkbdShow(s: VirtualKeyboard): [number, number, number, number];
	swkbdHide(s: VirtualKeyboard): void;
	swkbdUpdate(this: VirtualKeyboard): void;

	// tcp.c
	connect(cb: Callback<number>, ip: string, port: number): void;
	write(cb: Callback<number>, fd: number, data: ArrayBuffer): void;
	read(cb: Callback<number>, fd: number, buffer: ArrayBuffer): void;
	close(fd: number): void;
	tcpServerInit(c: any): void;
	tcpServerNew(
		ip: string,
		port: number,
		onAccept: (fd: number) => void
	): Server;

	// tls.c
	tlsHandshake(
		cb: Callback<TlsContextOpaque>,
		fd: number,
		hostname: string
	): void;
	tlsWrite(
		cb: Callback<number>,
		ctx: TlsContextOpaque,
		data: ArrayBuffer
	): void;
	tlsRead(
		cb: Callback<number>,
		ctx: TlsContextOpaque,
		buffer: ArrayBuffer
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
