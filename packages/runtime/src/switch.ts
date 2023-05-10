export const SWITCH_SYMBOL = Symbol('Switch');

export interface Native {
	cwd: () => string;
	getenv: (name: string) => string;
	setenv: (name: string, value: string) => void;
	envToObject: () => Record<string, string>;
	readDirSync: (path: string) => string[];
}

export class Switch extends EventTarget {
	env: Env;
	native: Native;

	// Populated by the host process
	exit!: () => void;
	print!: (str: string) => void;

	constructor() {
		super();
		this.env = new Env(this);
		// @ts-expect-error Populated by the host process
		this.native = {};
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
	[SWITCH_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[SWITCH_SYMBOL] = s;
	}

	get(name: string): string | undefined {
		return this[SWITCH_SYMBOL].native.getenv(name);
	}

	set(name: string, value: string): void {
		this[SWITCH_SYMBOL].native.setenv(name, value);
	}

	delete(name: string): void {}

	toObject(): Record<string, string> {
		return this[SWITCH_SYMBOL].native.envToObject();
	}
}
