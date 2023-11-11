import { $ } from './$';
import { Canvas, CanvasRenderingContext2D, ctxInternal } from './canvas';
import { FontFaceSet } from './polyfills/font';
import { type Callback, INTERNAL_SYMBOL, type Opaque } from './internal';
import { inspect } from './inspect';
import { bufferSourceToArrayBuffer, toPromise } from './utils';
import { setTimeout, clearTimeout } from './timers';
import { encoder } from './polyfills/text-encoder';
import { Socket, connect, createServer, parseAddress } from './tcp';
import { resolve as dnsResolve } from './dns';
import type {
	PathLike,
	Stats,
	ListenOpts,
	SocketAddress,
	SocketOptions,
} from './types';

export type CanvasRenderingContext2DState =
	Opaque<'CanvasRenderingContext2DState'>;
export type FontFaceState = Opaque<'FontFaceState'>;
export type ImageOpaque = Opaque<'ImageOpaque'>;
export type WasmModuleOpaque = Opaque<'WasmModuleOpaque'>;
export type WasmInstanceOpaque = Opaque<'WasmInstanceOpaque'>;
export type WasmGlobalOpaque = Opaque<'WasmGlobalOpaque'>;

export interface Vibration {
	duration: number;
	lowAmp: number;
	lowFreq: number;
	highAmp: number;
	highFreq: number;
}

type Keys = {
	modifiers: bigint;
	[i: number]: bigint;
};

/**
 * @private
 */
export interface Native {
	print(str: string): void;
	cwd(): string;
	chdir(dir: string): void;
	getInternalPromiseState(p: Promise<unknown>): [number, unknown];
	getenv(name: string): string | undefined;
	setenv(name: string, value: string): void;
	unsetenv(name: string): void;
	envToObject(): Record<string, string>;
	consoleInit(): void;
	consoleExit(): void;
	framebufferInit(buf: CanvasRenderingContext2DState): void;
	framebufferExit(): void;

	// applet
	appletGetAppletType(): number;
	appletGetOperationMode(): number;

	// hid
	hidInitializeKeyboard(): void;
	hidInitializeTouchScreen(): void;
	hidInitializeVibrationDevices(): void;
	hidGetTouchScreenStates(): Touch[] | undefined;
	hidGetKeyboardStates(): Keys;
	hidSendVibrationValues(v: VibrationValues): void;

	// fs
	readFile(cb: Callback<ArrayBuffer>, path: string): void;
	readDirSync(path: string): string[];
	readFileSync(path: string): ArrayBuffer;
	writeFileSync(path: string, data: ArrayBuffer): void;
	remove(cb: Callback<void>, path: string): void;
	stat(cb: Callback<Stats>, path: string): void;

	// font
	newFontFace(data: ArrayBuffer): FontFaceState;
	getSystemFont(): ArrayBuffer;

	// image
	decodeImage(
		cb: Callback<{
			opaque: ImageOpaque;
			width: number;
			height: number;
		}>,
		b: ArrayBuffer
	): void;

	// canvas
	canvasNewContext(w: number, h: number): CanvasRenderingContext2DState;
	canvasGetGlobalAlpha(ctx: CanvasRenderingContext2DState): number;
	canvasSetGlobalAlpha(ctx: CanvasRenderingContext2DState, n: number): void;
	canvasGetLineWidth(ctx: CanvasRenderingContext2DState): number;
	canvasSetLineWidth(ctx: CanvasRenderingContext2DState, n: number): void;
	canvasGetLineJoin(ctx: CanvasRenderingContext2DState): CanvasLineJoin;
	canvasSetLineJoin(
		ctx: CanvasRenderingContext2DState,
		n: CanvasLineJoin
	): void;
	canvasGetLineCap(ctx: CanvasRenderingContext2DState): CanvasLineCap;
	canvasSetLineCap(
		ctx: CanvasRenderingContext2DState,
		n: CanvasLineCap
	): void;
	canvasGetLineDash(ctx: CanvasRenderingContext2DState): number[];
	canvasSetLineDash(ctx: CanvasRenderingContext2DState, n: number[]): void;
	canvasGetLineDashOffset(ctx: CanvasRenderingContext2DState): number;
	canvasSetLineDashOffset(
		ctx: CanvasRenderingContext2DState,
		n: number
	): void;
	canvasGetMiterLimit(ctx: CanvasRenderingContext2DState): number;
	canvasSetMiterLimit(ctx: CanvasRenderingContext2DState, n: number): void;
	canvasBeginPath(ctx: CanvasRenderingContext2DState): void;
	canvasClosePath(ctx: CanvasRenderingContext2DState): void;
	canvasClip(
		ctx: CanvasRenderingContext2DState,
		fillRule?: CanvasFillRule
	): void;
	canvasFill(ctx: CanvasRenderingContext2DState): void;
	canvasStroke(ctx: CanvasRenderingContext2DState): void;
	canvasSave(ctx: CanvasRenderingContext2DState): void;
	canvasRestore(ctx: CanvasRenderingContext2DState): void;
	canvasMoveTo(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number
	): void;
	canvasLineTo(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number
	): void;
	canvasBezierCurveTo(
		ctx: CanvasRenderingContext2DState,
		cp1x: number,
		cp1y: number,
		cp2x: number,
		cp2y: number,
		x: number,
		y: number
	): void;
	canvasQuadraticCurveTo(
		ctx: CanvasRenderingContext2DState,
		cpx: number,
		cpy: number,
		x: number,
		y: number
	): void;
	canvasArc(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number,
		radius: number,
		startAngle: number,
		endAngle: number,
		counterclockwise: boolean
	): void;
	canvasArcTo(
		ctx: CanvasRenderingContext2DState,
		x1: number,
		y1: number,
		x2: number,
		y2: number,
		radius: number
	): void;
	canvasEllipse(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number,
		radiusX: number,
		radiusY: number,
		rotation: number,
		startAngle: number,
		endAngle: number,
		counterclockwise: boolean
	): void;
	canvasRect(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number,
		w: number,
		h: number
	): void;
	canvasRotate(ctx: CanvasRenderingContext2DState, n: number): void;
	canvasScale(ctx: CanvasRenderingContext2DState, x: number, y: number): void;
	canvasTranslate(
		ctx: CanvasRenderingContext2DState,
		x: number,
		y: number
	): void;
	canvasTransform(
		ctx: CanvasRenderingContext2DState,
		a: number,
		b: number,
		c: number,
		d: number,
		e: number,
		f: number
	): void;
	canvasGetTransform(ctx: CanvasRenderingContext2DState): number[];
	canvasResetTransform(ctx: CanvasRenderingContext2DState): void;
	canvasSetSourceRgba(
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
	canvasStrokeRect(
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
		sourceWidth: number,
		sourceHeight: number,
		dx: number,
		dy: number,
		dirtyX: number,
		dirtyY: number,
		dirtyWidth: number,
		dirtyHeight: number
	): void;
	canvasDrawImage(
		ctx: CanvasRenderingContext2DState,
		image: CanvasRenderingContext2DState | ImageOpaque,
		imageWidth: number,
		imageHeight: number,
		sx: number,
		sy: number,
		sw: number,
		sh: number,
		dx: number,
		dy: number,
		dw: number,
		dh: number,
		isCanvas: boolean
	): void;

	// crypto
	cryptoRandomBytes(buf: ArrayBuffer, offset: number, length: number): void;

	// wasm
	wasmNewModule(b: ArrayBuffer): WasmModuleOpaque;
	wasmNewInstance(
		m: WasmModuleOpaque,
		imports: any[]
	): [WasmInstanceOpaque, any[]];
	wasmNewGlobal(): WasmGlobalOpaque;
	wasmModuleExports(m: WasmModuleOpaque): any[];
	wasmModuleImports(m: WasmModuleOpaque): any[];
	wasmGlobalGet(g: WasmGlobalOpaque): any;
	wasmGlobalSet(g: WasmGlobalOpaque, v: any): void;
}

interface Internal {
	previousButtons: number;
	previousKeys: Keys;
	previousTouches: Touch[];
	keyboardInitialized?: boolean;
	touchscreenInitialized?: boolean;
	vibrationDevicesInitialized?: boolean;
	vibrationPattern?: (number | Vibration)[];
	vibrationTimeoutId?: number;
	renderingMode?: RenderingMode;
	nifmInitialized?: boolean;
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

export interface Versions {
	cairo: string;
	freetype2: string;
	nxjs: string;
	png: string;
	quickjs: string;
	turbojpeg: string;
	wasm3: string;
	webp: string;
}

type VibrationValues = Omit<Vibration, 'duration'>;
const DEFAULT_VIBRATION: VibrationValues = {
	lowAmp: 0.2,
	lowFreq: 160,
	highAmp: 0.2,
	highFreq: 320,
};

const STOP_VIBRATION: VibrationValues = {
	lowAmp: 0,
	lowFreq: 160,
	highAmp: 0,
	highFreq: 320,
};

export class SwitchClass extends EventTarget {
	/**
	 * A Map-like object providing methods to interact with the environment variables of the process.
	 */
	env: Env;
	/**
	 * An instance of {@link Canvas} that will result in drawing to the screen.
	 *
	 * The `width` and `height` properties are set to the Switch screen resolution.
	 *
	 * @note Calling the `getContext('2d')` method on this canvas switches to _canvas rendering mode_. When in this mode, avoid using any {@link console} methods, as they will switch back to _text rendering mode_.
	 */
	screen: Canvas;
	/**
	 * @ignore
	 */
	native: Native;
	/**
	 * Contains the available fonts for use on the screen Canvas context.
	 * By default, `"system-ui"` is the only font available, which is the system font provided by the Switch operating system.
	 *
	 * @demo See the [fonts](../apps/fonts/) application for an example of using custom fonts.
	 */
	fonts: FontFaceSet;
	/**
	 * @ignore
	 */
	[INTERNAL_SYMBOL]: Internal;

	// The following props are populated by the host process
	/**
	 * Array of the arguments passed to the process. Under normal circumstances, this array contains a single entry with the absolute path to the `.nro` file.
	 * @example [ "sdmc:/switch/nxjs.nro" ]
	 */
	argv!: string[];
	/**
	 * String value of the entrypoint JavaScript file that was evaluated. If a `main.js` file is present on the application's RomFS, then that will be executed first, in which case the value will be `romfs:/main.js`. Otherwise, the value will be the path of the `.nro` file on the SD card, with the `.nro` extension replaced with `.js`.
	 * @example "romfs:/main.js"
	 * @example "sdmc:/switch/nxjs.js"
	 */
	entrypoint!: string;
	/**
	 * An Object containing the versions numbers of nx.js and all supporting C libraries.
	 */
	version!: Versions;
	/**
	 * Signals for the nx.js application process to exit. The "exit" event will be invoked once the event loop is stopped.
	 */
	exit!: () => never;

	/**
	 * @private
	 */
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
	 * Prints the string `str` to the console, _without_ a trailing newline.
	 *
	 * > You will usually want to use the {@link console} methods instead.
	 *
	 * @note Invoking this method switches the application to _text rendering mode_,
	 * which clears any pixels previously drawn on the screen using the Canvas API.
	 */
	print(str: string) {
		const internal = this[INTERNAL_SYMBOL];
		if (internal.renderingMode !== RenderingMode.Console) {
			internal.setRenderingMode(RenderingMode.Console);
		}
		this.native.print(str);
	}

	/**
	 * Returns the current working directory as a URL string with a trailing slash.
	 *
	 * @example "sdmc:/switch/"
	 */
	cwd() {
		return `${this.native.cwd()}/`;
	}

	/**
	 * Changes the current working directory to the specified path.
	 *
	 * @example
	 *
	 * ```typescript
	 * Switch.chdir('sdmc:/switch/awesome-app/images');
	 * ```
	 */
	chdir(dir: PathLike) {
		return this.native.chdir(String(dir));
	}

	/**
	 * Performs a DNS lookup to resolve a hostname to an array of IP addresses.
	 *
	 * @example
	 *
	 * ```typescript
	 * const ipAddresses = await Switch.resolveDns('example.com');
	 * ```
	 */
	resolveDns(hostname: string) {
		return dnsResolve(hostname);
	}

	/**
	 * Returns a Promise which resolves to an `ArrayBuffer` containing
	 * the contents of the file at `path`.
	 *
	 * @example
	 *
	 * ```typescript
	 * const buffer = await Switch.readFile('sdmc:/switch/awesome-app/state.json');
	 * const gameState = JSON.parse(new TextDecoder().decode(buffer));
	 * ```
	 */
	readFile(path: PathLike) {
		return toPromise(this.native.readFile, String(path));
	}

	/**
	 * Synchronously returns an array of the file names within `path`.
	 *
	 * @example
	 *
	 * ```typescript
	 * for (const file of Switch.readDirSync('sdmc:/')) {
	 *   // … do something with `file` …
	 * }
	 * ```
	 */
	readDirSync(path: PathLike) {
		return this.native.readDirSync(String(path));
	}

	/**
	 * Synchronously returns an `ArrayBuffer` containing the contents
	 * of the file at `path`.
	 *
	 * @example
	 *
	 * ```typescript
	 * const buffer = Switch.readFileSync('sdmc:/switch/awesome-app/state.json');
	 * const appState = JSON.parse(new TextDecoder().decode(buffer));
	 * ```
	 */
	readFileSync(path: PathLike) {
		return this.native.readFileSync(String(path));
	}

	/**
	 * Synchronously writes the contents of `data` to the file at `path`.
	 *
	 * ```typescript
	 * const appStateJson = JSON.stringify(appState);
	 * Switch.writeFileSync('sdmc:/switch/awesome-app/state.json', appStateJson);
	 * ```
	 */
	writeFileSync(path: PathLike, data: string | BufferSource) {
		const d = typeof data === 'string' ? encoder.encode(data) : data;
		const ab = bufferSourceToArrayBuffer(d);
		return this.native.writeFileSync(String(path), ab);
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
	 * Creates a TCP connection to the specified `address`.
	 *
	 * @param address Hostname and port number of the destination TCP server to connect to.
	 * @param opts Optional
	 * @see https://sockets-api.proposal.wintercg.org
	 */
	connect<Host extends string, Port extends string>(
		address: `${Host}:${Port}` | SocketAddress,
		opts?: SocketOptions
	) {
		return new Socket(
			// @ts-expect-error Internal constructor
			INTERNAL_SYMBOL,
			typeof address === 'string' ? parseAddress(address) : address,
			{ ...opts, connect }
		);
	}

	listen(opts: ListenOpts) {
		const { ip = '0.0.0.0', port } = opts;
		return createServer(ip, port);
	}

	networkInfo() {
		if (!this[INTERNAL_SYMBOL].nifmInitialized) {
			addEventListener('unload', $.nifmInitialize());
			this[INTERNAL_SYMBOL].nifmInitialized = true;
		}
		return $.networkInfo();
	}

	/**
	 * Vibrates the main gamepad for the specified number of milliseconds or pattern.
	 *
	 * If a vibration pattern is already in progress when this method is called,
	 * the previous pattern is halted and the new one begins instead.
	 *
	 * @example
	 *
	 * ```typescript
	 * // Vibrate for 200ms with the default amplitude/frequency values
	 * Switch.vibrate(200);
	 *
	 * // Vibrate 'SOS' in Morse Code
	 * Switch.vibrate([
	 *   100, 30, 100, 30, 100, 30, 200, 30, 200, 30, 200, 30, 100, 30, 100, 30, 100,
	 * ]);
	 *
	 * // Specify amplitude/frequency values for the vibration
	 * Switch.vibrate({
	 *   duration: 500,
	 *   lowAmp: 0.2
	 *   lowFreq: 160,
	 *   highAmp: 0.6,
	 *   highFreq: 500
	 * });
	 * ```
	 *
	 * @param pattern Provides a pattern of vibration and pause intervals. Each value indicates a number of milliseconds to vibrate or pause, in alternation. You may provide either a single value (to vibrate once for that many milliseconds) or an array of values to alternately vibrate, pause, then vibrate again.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/vibrate
	 */
	vibrate(pattern: number | Vibration | (number | Vibration)[]): boolean {
		if (!Array.isArray(pattern)) {
			pattern = [pattern];
		}
		const patternValues: (number | Vibration)[] = [];
		for (let i = 0; i < pattern.length; i++) {
			let p = pattern[i];
			if (i % 2 === 0) {
				// Even - vibration interval
				if (typeof p === 'number') {
					p = {
						...DEFAULT_VIBRATION,
						duration: p,
					};
				}
				if (
					p.highAmp < 0 ||
					p.highAmp > 1 ||
					p.lowAmp < 0 ||
					p.lowAmp > 1
				) {
					return false;
				}
				patternValues.push(p);
			} else {
				// Odd - pause interval
				if (typeof p !== 'number') return false;
				patternValues.push(p);
			}
		}
		const internal = this[INTERNAL_SYMBOL];
		if (!internal.vibrationDevicesInitialized) {
			this.native.hidInitializeVibrationDevices();
			this.native.hidSendVibrationValues(DEFAULT_VIBRATION);
			internal.vibrationDevicesInitialized = true;
		}
		if (typeof internal.vibrationTimeoutId === 'number') {
			clearTimeout(internal.vibrationTimeoutId);
		}
		internal.vibrationPattern = patternValues;
		internal.vibrationTimeoutId = setTimeout(this.#processVibrations, 0);
		return true;
	}

	/**
	 * @ignore
	 */
	#processVibrations = () => {
		const internal = this[INTERNAL_SYMBOL];
		let next = internal.vibrationPattern?.shift();
		if (typeof next === 'undefined') {
			// Pattern completed
			next = 0;
		}
		if (typeof next === 'number') {
			// Pause interval
			this.native.hidSendVibrationValues(STOP_VIBRATION);
			if (next > 0) {
				internal.vibrationTimeoutId = setTimeout(
					this.#processVibrations,
					next
				);
			}
		} else {
			// Vibration interval
			this.native.hidSendVibrationValues(next);
			internal.vibrationTimeoutId = setTimeout(
				this.#processVibrations,
				next.duration
			);
		}
	};

	inspect = inspect;
}

export class Env {
	/**
	 * @private
	 */
	[INTERNAL_SYMBOL]: SwitchClass;

	/**
	 * @private
	 */
	constructor(s: SwitchClass) {
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
	[INTERNAL_SYMBOL]: SwitchClass;

	constructor(s: SwitchClass, w: number, h: number) {
		super(w, h);
		this[INTERNAL_SYMBOL] = s;
	}

	getContext(contextId: '2d'): CanvasRenderingContext2D {
		const ctx = super.getContext(contextId);
		const Switch = this[INTERNAL_SYMBOL];
		const internal = Switch[INTERNAL_SYMBOL];
		if (internal.renderingMode !== RenderingMode.Framebuffer) {
			internal.setRenderingMode(
				RenderingMode.Framebuffer,
				ctxInternal(ctx).ctx
			);
		}
		return ctx;
	}
}
