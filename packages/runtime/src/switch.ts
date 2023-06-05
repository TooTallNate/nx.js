import { Canvas, CanvasRenderingContext2D } from './canvas';
import { FontFaceSet } from './font';
import { INTERNAL_SYMBOL } from './types';

export type Opaque<T> = { __type: T };
export type CanvasRenderingContext2DState =
	Opaque<'CanvasRenderingContext2DState'>;
export type FontFaceState = Opaque<'FontFaceState'>;

type Keys = {
	modifiers: bigint;
	[i: number]: bigint;
};

export interface Native {
	print: (str: string) => void;
	cwd: () => string;
	getenv: (name: string) => string;
	setenv: (name: string, value: string) => void;
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

	// fs
	readDirSync: (path: string) => string[];
	readFileSync: (path: string) => ArrayBuffer;

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

export class Switch extends EventTarget {
	env: Env;
	screen: Canvas;
	native: Native;
	fonts: FontFaceSet;
	[INTERNAL_SYMBOL]: Internal;

	// Populated by the host process
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
					//native.consoleExit();
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
	 * Returns an array of the file names within `path`.
	 */
	readDirSync(path: string | URL) {
		return this.native.readDirSync(String(path));
	}

	/**
	 * Returns an `ArrayBuffer` containing the contents of the file at `path`.
	 */
	readFileSync(path: string | URL) {
		return this.native.readFileSync(String(path));
	}
}

export class Env {
	[INTERNAL_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[INTERNAL_SYMBOL] = s;
	}

	get(name: string): string | undefined {
		return this[INTERNAL_SYMBOL].native.getenv(name);
	}

	set(name: string, value: string): void {
		this[INTERNAL_SYMBOL].native.setenv(name, value);
	}

	delete(name: string): void {}

	toObject(): Record<string, string> {
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
