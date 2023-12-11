import { $ } from './$';
import { FontFaceSet } from './font/font-face-set';
import { type Callback, INTERNAL_SYMBOL, type Opaque } from './internal';
import { Env } from './env';
import { inspect } from './inspect';
import { bufferSourceToArrayBuffer, toPromise, pathToString } from './utils';
import { setTimeout, clearTimeout } from './timers';
import { encoder } from './polyfills/text-encoder';
import { EventTarget } from './polyfills/event-target';
import { Socket, connect, createServer, parseAddress } from './tcp';
import { resolve as dnsResolve } from './dns';
import type { Screen as Screen } from './screen';
import type {
	PathLike,
	Stats,
	ListenOpts,
	SocketAddress,
	SocketOptions,
} from './types';

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
	cwd(): string;
	chdir(dir: string): void;
	consoleInit(): void;
	consoleExit(): void;
	framebufferInit(buf: Screen): void;
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
	setRenderingMode: (mode: RenderingMode, ctx?: Screen) => void;
	cleanup: () => void;
}

export enum RenderingMode {
	Init,
	Console,
	Framebuffer,
}

interface SwitchEventHandlersEventMap {
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
	harfbuzz: string;
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
		this.env = new Env();
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
			setRenderingMode(mode: RenderingMode, ctx?: Screen) {
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

		// TODO: Move to `document`
		// @ts-expect-error Internal constructor
		this.fonts = new FontFaceSet(INTERNAL_SYMBOL);
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
		$.print(str);
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
		return this.native.chdir(pathToString(dir));
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
		return toPromise(this.native.readFile, pathToString(path));
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
		return this.native.readDirSync(pathToString(path));
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
		return this.native.readFileSync(pathToString(path));
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
		return this.native.writeFileSync(pathToString(path), ab);
	}

	/**
	 * Removes the file or directory specified by `path`.
	 */
	remove(path: PathLike) {
		return toPromise(this.native.remove, pathToString(path));
	}

	/**
	 * Returns a Promise which resolves to an object containing
	 * information about the file pointed to by `path`.
	 */
	stat(path: PathLike) {
		return toPromise(this.native.stat, pathToString(path));
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
