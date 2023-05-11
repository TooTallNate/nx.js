export const INTERNAL_SYMBOL = Symbol('Internal');

export interface Native {
	print: (str: string) => void;
	cwd: () => string;
	getenv: (name: string) => string;
	setenv: (name: string, value: string) => void;
	envToObject: () => Record<string, string>;
	readDirSync: (path: string) => string[];
	consoleInit: () => void;
	consoleExit: () => void;
}

interface Internal {
	renderingMode?: RenderingMode;
	setRenderingMode: (mode: RenderingMode) => void;
	cleanup: () => void;
}

enum RenderingMode {
	Init,
	Console,
	Framebuffer,
}

export class Switch extends EventTarget {
	env: Env;
	//canvas: Canvas;
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
			setRenderingMode(mode: RenderingMode) {
				native.consoleInit();
				this.renderingMode = mode;
			},
			cleanup() {
				if (this.renderingMode === RenderingMode.Console) {
					native.consoleExit();
				}
			},
		};
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
