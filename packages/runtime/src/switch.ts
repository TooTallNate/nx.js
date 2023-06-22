import { Canvas, CanvasRenderingContext2D } from './canvas';
import { FontFaceSet } from './font';
import { INTERNAL_SYMBOL, PathLike, Stats } from './types';
import { inspect } from './inspect';

export type Opaque<T> = { __type: T };
export type CanvasRenderingContext2DState =
	Opaque<'CanvasRenderingContext2DState'>;
export type FontFaceState = Opaque<'FontFaceState'>;

type Keys = {
	modifiers: bigint;
	[i: number]: bigint;
};

type Callback<T> = (err: Error | null, result: T) => void;
type CallbackReturnType<T> = T extends (
	fn: Callback<infer U>,
	...args: any[]
) => any
	? U
	: never;
type CallbackArguments<T> = T extends (
	fn: Callback<any>,
	...args: infer U
) => any
	? U
	: never;

interface ConnectOpts {
	hostname?: string;
	port: number;
}

export interface Native {
	print: (str: string) => void;
	cwd: () => string;
	chdir: (dir: string) => void;
	getInternalPromiseState: (p: Promise<unknown>) => [number, unknown];
	getenv: (name: string) => string | undefined;
	setenv: (name: string, value: string) => void;
	unsetenv: (name: string) => void;
	envToObject: () => Record<string, string>;
	consoleInit: () => void;
	consoleExit: () => void;
	framebufferInit: (buf: CanvasRenderingContext2DState) => void;
	framebufferExit: () => void;

	// applet
	appletGetAppletType: () => number;
	appletGetOperationMode: () => number;

	// hid
	hidInitializeKeyboard: () => void;
	hidInitializeTouchScreen: () => void;
	hidGetTouchScreenStates: () => Touch[] | undefined;
	hidGetKeyboardStates: () => Keys;

	// dns
	resolveDns: (cb: Callback<string[]>, hostname: string) => void;

	// fs
	readFile: (cb: Callback<ArrayBuffer>, path: string) => void;
	readDirSync: (path: string) => string[];
	readFileSync: (path: string) => ArrayBuffer;
	remove: (cb: Callback<void>, path: string) => void;
	stat: (cb: Callback<Stats>, path: string) => void;

	// font
	newFontFace: (data: ArrayBuffer) => FontFaceState;
	getSystemFont: () => ArrayBuffer;

	// canvas
	canvasNewContext(
		width: number,
		height: number
	): CanvasRenderingContext2DState;
	canvasSetLineWidth(ctx: CanvasRenderingContext2DState, n: number): void;
	canvasSetFillStyle(
		ctx: CanvasRenderingContext2DState,
		r: number,
		g: number,
		b: number,
		a: number
	): void;
	canvasSetFont(
		ctx: CanvasRenderingContext2DState,
		face: FontFaceState,
		fontSize: number
	): void;
	canvasFillRect(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number,
		w: number,
		h: number
	): void;
	canvasFillText(
		ctx: CanvasRenderingContext2DState,
		text: string,
		x: number,
		y: number,
		maxWidth?: number | undefined
	): void;
	canvasMeasureText(ctx: CanvasRenderingContext2DState, text: string): any;
	canvasGetImageData(
		ctx: CanvasRenderingContext2DState,
		sx: number,
		sy: number,
		sw: number,
		sh: number
	): ArrayBuffer;
	canvasPutImageData(
		ctx: CanvasRenderingContext2DState,
		source: ArrayBuffer,
		dx: number,
		dy: number,
		dirtyX: number,
		dirtyY: number,
		dirtyWidth: number,
		dirtyHeight: number
	): void;

	// tcp
	connect(cb: Callback<number>, ip: string, port: number): void;
	write(cb: Callback<number>, fd: number, data: ArrayBuffer): void;
	read(cb: Callback<number>, fd: number, buffer: ArrayBuffer): void;
}

interface Internal {
	previousButtons: number;
	previousKeys: Keys;
	previousTouches: Touch[];
	keyboardInitialized?: boolean;
	touchscreenInitialized?: boolean;
	renderingMode?: RenderingMode;
	setRenderingMode: (
		mode: RenderingMode,
		ctx?: CanvasRenderingContext2DState
	) => void;
	cleanup: () => void;
}

enum RenderingMode {
	Init,
	Console,
	Framebuffer,
}

interface SwitchEventHandlersEventMap {
	frame: UIEvent;
	exit: Event;
	buttondown: UIEvent;
	buttonup: UIEvent;
	keydown: KeyboardEvent;
	keyup: KeyboardEvent;
	touchstart: TouchEvent;
	touchmove: TouchEvent;
	touchend: TouchEvent;
}

interface Versions {
	nxjs: string;
	cairo: string;
	freetype2: string;
	quickjs: string;
}

//const encoder = new TextEncoder();

function toPromise<Func extends (cb: Callback<any>, ...args: any[]) => any>(
	fn: Func,
	...args: CallbackArguments<Func>
) {
	return new Promise<CallbackReturnType<Func>>((resolve, reject) => {
		fn((err, result) => {
			if (err) return reject(err);
			resolve(result);
		}, ...args);
	});
}

export class Switch extends EventTarget {
	env: Env;
	screen: Canvas;
	native: Native;
	fonts: FontFaceSet;
	[INTERNAL_SYMBOL]: Internal;

	// Populated by the host process
	argv!: string[];
	entrypoint!: string;
	version!: Versions;
	exit!: () => never;

	constructor() {
		super();
		// @ts-expect-error Populated by the host process
		const native: Native = {};
		this.native = native;
		this.env = new Env(this);
		this[INTERNAL_SYMBOL] = {
			previousButtons: 0,
			previousTouches: [],
			previousKeys: {
				[0]: 0n,
				[1]: 0n,
				[2]: 0n,
				[3]: 0n,
				modifiers: 0n,
			},
			renderingMode: RenderingMode.Init,
			setRenderingMode(
				mode: RenderingMode,
				ctx?: CanvasRenderingContext2DState
			) {
				if (mode === RenderingMode.Console) {
					native.framebufferExit();
					native.consoleInit();
				} else if (mode === RenderingMode.Framebuffer && ctx) {
					native.consoleExit();
					native.framebufferInit(ctx);
				} else {
					throw new Error('Unsupported rendering mode');
				}
				this.renderingMode = mode;
			},
			cleanup() {
				if (this.renderingMode === RenderingMode.Console) {
					native.consoleExit();
				} else if (this.renderingMode === RenderingMode.Framebuffer) {
					native.framebufferExit();
				}
			},
		};

		// Framebuffer mode uses the HTML5 Canvas API
		this.screen = new Screen(this, 1280, 720);

		this.fonts = new FontFaceSet();
	}

	addEventListener<K extends keyof SwitchEventHandlersEventMap>(
		type: K,
		listener: (ev: SwitchEventHandlersEventMap[K]) => any,
		options?: boolean | AddEventListenerOptions
	): void;
	addEventListener(
		type: string,
		listener: EventListenerOrEventListenerObject,
		options?: boolean | AddEventListenerOptions
	): void;
	addEventListener(
		type: string,
		callback: EventListenerOrEventListenerObject | null,
		options?: boolean | AddEventListenerOptions | undefined
	): void {
		if (
			!this[INTERNAL_SYMBOL].keyboardInitialized &&
			(type === 'keydown' || type === 'keyup')
		) {
			this.native.hidInitializeKeyboard();
			this[INTERNAL_SYMBOL].keyboardInitialized = true;
		}
		if (
			!this[INTERNAL_SYMBOL].touchscreenInitialized &&
			(type === 'touchstart' ||
				type === 'touchmove' ||
				type === 'touchend')
		) {
			this.native.hidInitializeTouchScreen();
			this[INTERNAL_SYMBOL].touchscreenInitialized = true;
		}
		super.addEventListener(type, callback, options);
	}

	removeEventListener<K extends keyof SwitchEventHandlersEventMap>(
		type: K,
		listener: (ev: SwitchEventHandlersEventMap[K]) => any,
		options?: boolean | EventListenerOptions
	): void;
	removeEventListener(
		type: string,
		listener: EventListenerOrEventListenerObject,
		options?: boolean | EventListenerOptions
	): void;
	removeEventListener(
		type: string,
		listener: EventListenerOrEventListenerObject,
		options?: boolean | EventListenerOptions
	): void {
		super.removeEventListener(type, listener, options);
	}

	/**
	 * Prints `str` to the console.
	 */
	print(str: string) {
		const internal = this[INTERNAL_SYMBOL];
		if (internal.renderingMode !== RenderingMode.Console) {
			internal.setRenderingMode(RenderingMode.Console);
		}
		this.native.print(str);
	}

	/**
	 * Returns the current working directory as a URL instance.
	 */
	cwd() {
		return new URL(`${this.native.cwd()}/`);
	}

	/**
	 * Changes the current working directory to the specified path.
	 */
	chdir(dir: PathLike) {
		return this.native.chdir(String(dir));
	}

	/**
	 * Performs a DNS lookup to resolve a hostname to an array of IP addresses.
	 */
	resolveDns(hostname: string) {
		return toPromise(this.native.resolveDns, hostname);
	}

	/**
	 * Returns a Promise which resolves to an `ArrayBuffer` containing
	 * the contents of the file at `path`.
	 */
	readFile(path: PathLike) {
		return toPromise(this.native.readFile, String(path));
	}

	/**
	 * Synchronously returns an array of the file names within `path`.
	 */
	readDirSync(path: PathLike) {
		return this.native.readDirSync(String(path));
	}

	/**
	 * Synchronously returns an `ArrayBuffer` containing the contents
	 * of the file at `path`.
	 */
	readFileSync(path: PathLike) {
		return this.native.readFileSync(String(path));
	}

	/**
	 * Removes the file or directory specified by `path`.
	 */
	remove(path: PathLike) {
		return toPromise(this.native.remove, String(path));
	}

	/**
	 * Returns a Promise which resolves to an object containing
	 * information about the file pointed to by `path`.
	 */
	stat(path: PathLike) {
		return toPromise(this.native.stat, String(path));
	}

	/**
	 * Creates a TCP connection specified by the `hostname`
	 * and `port`.
	 */
	async connect({ hostname = '127.0.0.1', port }: ConnectOpts) {
		const [ip] = await this.resolveDns(hostname);
		if (!ip) {
			throw new Error(`Could not resolve "${hostname}" to an IP address`);
		}
		return toPromise(this.native.connect, ip, port);
	}

	read(fd: number, buffer: ArrayBuffer) {
		//const ab = bufferSourceToArrayBuffer(buffer);
		return toPromise(this.native.read, fd, buffer);
	}

	write(fd: number, data: ArrayBuffer) {
		//const d = typeof data === 'string' ? encoder.encode(data) : data;
		//const ab = bufferSourceToArrayBuffer(data);
		return toPromise(this.native.write, fd, data);
	}

	inspect = inspect;
}

export class Env {
	[INTERNAL_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[INTERNAL_SYMBOL] = s;
	}

	get(name: string) {
		return this[INTERNAL_SYMBOL].native.getenv(name);
	}

	set(name: string, value: string) {
		this[INTERNAL_SYMBOL].native.setenv(name, value);
	}

	delete(name: string) {
		this[INTERNAL_SYMBOL].native.unsetenv(name);
	}

	toObject() {
		return this[INTERNAL_SYMBOL].native.envToObject();
	}
}

class Screen extends Canvas {
	getContext(contextId: '2d'): CanvasRenderingContext2D {
		const ctx = super.getContext(contextId);
		const Switch = this[INTERNAL_SYMBOL];
		const internal = Switch[INTERNAL_SYMBOL];
		if (internal.renderingMode !== RenderingMode.Framebuffer) {
			internal.setRenderingMode(
				RenderingMode.Framebuffer,
				ctx[INTERNAL_SYMBOL]
			);
		}
		return ctx;
	}
}
