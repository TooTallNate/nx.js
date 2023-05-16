import { Canvas, CanvasRenderingContext2D } from './canvas';

export const INTERNAL_SYMBOL = Symbol('Internal');

export enum AppletOperationMode {
	Handheld = 0, ///< Handheld
	Console = 1, ///< Console (Docked / TV-mode)
}

export interface Native {
	print: (str: string) => void;
	cwd: () => string;
	getenv: (name: string) => string;
	setenv: (name: string, value: string) => void;
	envToObject: () => Record<string, string>;
	readDirSync: (path: string) => string[];
	consoleInit: () => void;
	consoleExit: () => void;
	framebufferInit: (buf: ArrayBuffer) => void;
	framebufferExit: () => void;
	appletGetOperationMode: () => AppletOperationMode;

	// canvas
	canvasNewContext: (width: number, height: number) => any;
	canvasFillRect(ctx: any, x: number, y: number, w: number, h: number): void;
	canvasGetImageData: (
		ctx: any,
		sx: number,
		sy: number,
		sw: number,
		sh: number
	) => ArrayBuffer;
	canvasPutImageData: (
		ctx: any,
		source: ArrayBuffer,
		dx: number,
		dy: number,
		dirtyX: number,
		dirtyY: number,
		dirtyWidth: number,
		dirtyHeight: number
	) => void;
}

interface Internal {
	renderingMode?: RenderingMode;
	setRenderingMode: (mode: RenderingMode, buf?: ArrayBuffer) => void;
	cleanup: () => void;
}

enum RenderingMode {
	Init,
	Console,
	Framebuffer,
}

export class Switch extends EventTarget {
	env: Env;
	screen: Canvas;
	native: Native;
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
			renderingMode: RenderingMode.Init,
			setRenderingMode(mode: RenderingMode, ctx?: any) {
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
	cwd(): URL {
		return new URL(`${this.native.cwd()}/`);
	}

	/**
	 * Returns an array of the file names within `path`.
	 */
	readDirSync(path: string | URL): string[] {
		return this.native.readDirSync(String(path));
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
