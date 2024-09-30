import type {
	Album,
	AlbumFile,
	Application,
	FileSystem,
	IRSensor,
	NetworkInfo,
	Profile,
	ProfileUid,
	SaveData,
	SaveDataCreationInfo,
	SaveDataFilter,
	Stats,
	Versions,
} from './switch';
import type {
	Callback,
	Keys,
	Opaque,
	RGBA,
	VibrationValues,
	WasmGlobalOpaque,
	WasmInstanceOpaque,
	WasmModuleOpaque,
} from './internal';
import type { Touch } from './polyfills/event';
import type { Server, TlsContextOpaque } from './tcp';
import type { MemoryDescriptor, Memory } from './wasm';
import type { BatteryManager } from './navigator/battery';
import type { VirtualKeyboard } from './navigator/virtual-keyboard';
import type { OffscreenCanvas } from './canvas/offscreen-canvas';
import type { ImageBitmap } from './canvas/image-bitmap';
import type { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';
import type { OffscreenCanvasRenderingContext2D } from './canvas/offscreen-canvas-rendering-context-2d';
import type { Image } from './image';
import type { Screen } from './screen';
import type { FontFace } from './font/font-face';
import type { URL, URLSearchParams } from './polyfills/url';
import type { DOMPoint, DOMPointInit } from './dompoint';
import type { DOMMatrix, DOMMatrixReadOnly, DOMMatrixInit } from './dommatrix';
import type { Gamepad, GamepadButton } from './navigator/gamepad';
import type { PromiseState } from '@nx.js/inspect';

type ClassOf<T> = {
	new (...args: any[]): T;
};

type FileHandle = Opaque<'FileHandle'>;
type SaveDataIterator = Opaque<'SaveDataIterator'>;
type URLSearchParamsIterator = Opaque<'URLSearchParamsIterator'>;

export interface Init {
	// account.c
	accountInitialize(): () => void;
	accountProfileInit(c: ClassOf<Profile>): void;
	accountCurrentProfile(): Profile | null;
	accountSelectProfile(): Profile | null;
	accountProfileNew(uid: ProfileUid): Profile;
	accountProfiles(): Profile[];

	// album.c
	capsaInitialize(): () => void;
	albumInit(c: ClassOf<Album>): void;
	albumFileInit(c: ClassOf<AlbumFile>): void;
	albumFileList(album: Album): AlbumFile[];

	// applet.c
	appletIlluminance(): number;
	appletGetAppletType(): number;
	appletGetOperationMode(): number;
	appletSetMediaPlaybackState(state: boolean): void;

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
		c: ClassOf<CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D>,
	): void;
	canvasContext2dGetImageData(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		sx: number,
		sy: number,
		sw: number,
		sh: number,
	): ArrayBuffer;
	canvasContext2dGetTransform(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): number[];
	canvasContext2dGetFont(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): string;
	canvasContext2dSetFont(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		font: FontFace,
		size: number,
		fontString: string,
	): number[];
	canvasContext2dGetFillStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): RGBA;
	canvasContext2dSetFillStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): number[];
	canvasContext2dGetStrokeStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
	): RGBA;
	canvasContext2dSetStrokeStyle(
		ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D,
		...rgba: RGBA
	): number[];

	// crypto.c
	cryptoDigest(algorithm: string, buf: ArrayBuffer): Promise<ArrayBuffer>;
	cryptoRandomBytes(buf: ArrayBuffer, offset: number, length: number): void;
	sha256Hex(str: string): string;

	// dommatrix.c
	dommatrixNew(values?: number[]): DOMMatrix | DOMMatrixReadOnly;
	dommatrixFromMatrix(init?: DOMMatrixInit): DOMMatrix | DOMMatrixReadOnly;
	dommatrixROInitClass(c: ClassOf<DOMMatrixReadOnly>): void;
	dommatrixInitClass(c: ClassOf<DOMMatrix>): void;
	dommatrixTransformPoint(m: DOMMatrixReadOnly, p: DOMPointInit): DOMPoint;

	// dns.c
	dnsResolve(hostname: string): Promise<string[]>;

	// error.c
	onError(fn: (err: any) => number): void;
	onUnhandledRejection(
		fn: (promise: Promise<unknown>, reason: any) => number,
	): void;

	// font.c
	fontFaceNew(data: ArrayBuffer): FontFace;
	getSystemFont(type: number): ArrayBuffer;

	// fs.c
	fclose(f: FileHandle): Promise<void>;
	fopen(path: string, mode: string): Promise<FileHandle>;
	fread(f: FileHandle, buf: ArrayBuffer): Promise<number | null>;
	fwrite(f: FileHandle, data: ArrayBuffer): Promise<void>;
	fsCreateBigFile(path: string): void;
	mkdirSync(path: string, mode: number): number;
	readDirSync(path: string): string[] | null;
	readFile(path: string): Promise<ArrayBuffer | null>;
	readFileSync(path: string): ArrayBuffer | null;
	remove(path: string): Promise<void>;
	removeSync(path: string): void;
	rename(path: string, dest: string): Promise<void>;
	renameSync(path: string, dest: string): void;
	stat(path: string): Promise<Stats | null>;
	statSync(path: string): Stats | null;
	writeFileSync(path: string, data: ArrayBuffer): void;

	// fsdev.c
	fsInit(c: ClassOf<FileSystem>): void;
	fsMount(fs: FileSystem, name: string): void;
	fsOpenBis(id: number): FileSystem;
	saveDataInit(c: ClassOf<SaveData>): void;
	saveDataCreateSync(info: SaveDataCreationInfo, nacp?: ArrayBuffer): void;
	saveDataMount(saveData: SaveData, name: string): void;
	saveDataFilter(filter: SaveDataFilter): SaveDataIterator;
	fsOpenSaveDataInfoReader(saveDataSpaceId: number): SaveDataIterator | null;
	fsSaveDataInfoReaderNext(iterator: SaveDataIterator): SaveData | null;

	// gamepad.c
	gamepadInit(c: ClassOf<Gamepad>): void;
	gamepadNew(index: number): Gamepad;
	gamepadButtonInit(c: ClassOf<GamepadButton>): void;
	gamepadButtonNew(gamepad: Gamepad, index: number): void;

	// image.c
	imageInit(c: ClassOf<Image | ImageBitmap>): void;
	imageNew(width?: number, height?: number): Image | ImageBitmap;
	imageDecode(img: Image | ImageBitmap, data: ArrayBuffer): Promise<void>;
	imageClose(img: ImageBitmap): void;

	// irs.c
	irsInit(): () => void;
	irsSensorNew(image: ImageBitmap, color: RGBA): IRSensor;
	irsSensorStart(s: IRSensor): void;
	irsSensorStop(s: IRSensor): void;
	irsSensorUpdate(s: IRSensor): boolean;

	// main.c
	argv: string[];
	entrypoint: string;
	version: Versions;
	exit(): never;
	cwd(): string;
	chdir(dir: string): void;
	print(v: string): void;
	printErr(v: string): void;
	getInternalPromiseState(p: Promise<unknown>): [PromiseState, unknown];
	getenv(name: string): string | undefined;
	setenv(name: string, value: string): void;
	unsetenv(name: string): void;
	envToObject(): Record<string, string>;
	onFrame(fn: (plusDown: boolean) => void): void;
	onExit(fn: () => void): void;
	framebufferInit(screen: Screen): void;
	hidInitializeTouchScreen(): void;
	hidGetTouchScreenStates(): Touch[] | undefined;
	hidInitializeKeyboard(): void;
	hidInitializeVibrationDevices(): void;
	hidGetKeyboardStates(): Keys;
	hidSendVibrationValues(v: VibrationValues): void;

	// nifm.c
	nifmInitialize(): () => void;
	networkInfo(): NetworkInfo;

	// ns.c
	nsInitialize(): () => void;
	nsAppInit(c: ClassOf<Application>): void;
	nsAppNew(id: BigInt | ArrayBuffer | null): Application;
	nsAppNext(index: number): bigint | null;

	// software-keyboard.c
	swkbdCreate(fns: {
		onCancel: (this: VirtualKeyboard) => void;
		onChange: (
			this: VirtualKeyboard,
			str: string,
			cursorPos: number,
			dicStartCursorPos: number,
			dicEndCursorPos: number,
		) => void;
		onSubmit: (this: VirtualKeyboard, str: string) => void;
		onCursorMove: (
			this: VirtualKeyboard,
			str: string,
			cursorPos: number,
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
		onAccept: (fd: number) => void,
	): Server;

	// tls.c
	tlsHandshake(
		cb: Callback<TlsContextOpaque>,
		fd: number,
		hostname: string,
	): void;
	tlsWrite(
		cb: Callback<number>,
		ctx: TlsContextOpaque,
		data: ArrayBuffer,
	): void;
	tlsRead(
		cb: Callback<number>,
		ctx: TlsContextOpaque,
		buffer: ArrayBuffer,
	): void;

	// url.c
	urlInit(c: ClassOf<URL>): void;
	urlNew(url: string | URL, base?: string | URL): URL;
	urlSearchInit(c: ClassOf<URLSearchParams>): void;
	urlSearchNew(input: string, url?: URL): URLSearchParams;
	urlSearchIterator(
		params: URLSearchParams,
		type: number,
	): URLSearchParamsIterator;
	urlSearchIteratorNext(it: URLSearchParamsIterator): any;

	// wasm.c
	wasmCallFunc(f: any, ...args: unknown[]): unknown;
	wasmMemNew(descriptor: MemoryDescriptor): Memory;
	wasmTableGet(t: any, i: number): Memory;
	wasmInitMemory(c: any): void;
	wasmInitTable(c: any): void;
	wasmNewModule(b: ArrayBuffer): WasmModuleOpaque;
	wasmNewInstance(
		m: WasmModuleOpaque,
		imports: any[],
	): [WasmInstanceOpaque, any[]];
	wasmNewGlobal(): WasmGlobalOpaque;
	wasmModuleExports(m: WasmModuleOpaque): any[];
	wasmModuleImports(m: WasmModuleOpaque): any[];
	wasmGlobalGet(g: WasmGlobalOpaque): any;
	wasmGlobalSet(g: WasmGlobalOpaque, v: any): void;

	applyPath: any;
}

export const $: Init = (globalThis as any).$;
delete (globalThis as any).$;
