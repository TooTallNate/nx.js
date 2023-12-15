import { $ } from './$';
import {
	INTERNAL_SYMBOL,
	type Keys,
	type Vibration,
	type VibrationValues,
} from './internal';
import {
	readDirSync,
	readFile,
	readFileSync,
	remove,
	stat,
	writeFileSync,
} from './fs';
import { Env } from './env';
import { inspect } from './inspect';
import { pathToString } from './utils';
import { setTimeout, clearTimeout } from './timers';
import { EventTarget } from './polyfills/event-target';
import { Socket, connect, createServer, parseAddress } from './tcp';
import { resolve as dnsResolve } from './dns';
import type {
	PathLike,
	ListenOpts,
	SocketAddress,
	SocketOptions,
} from './types';

interface SwitchInternal {
	previousButtons: number;
	previousKeys: Keys;
	keyboardInitialized?: boolean;
	vibrationDevicesInitialized?: boolean;
	vibrationPattern?: (number | Vibration)[];
	vibrationTimeoutId?: number;
	nifmInitialized?: boolean;
}

interface SwitchEventHandlersEventMap {
	buttondown: UIEvent;
	buttonup: UIEvent;
	keydown: KeyboardEvent;
	keyup: KeyboardEvent;
}

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

export const internal: SwitchInternal = {
	previousButtons: 0,
	previousKeys: {
		[0]: 0n,
		[1]: 0n,
		[2]: 0n,
		[3]: 0n,
		modifiers: 0n,
	},
};

export class SwitchClass extends EventTarget {
	/**
	 * A Map-like object providing methods to interact with the environment variables of the process.
	 */
	env: Env;
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
	get entrypoint() {
		return $.entrypoint;
	}
	/**
	 * An Object containing the versions numbers of nx.js and all supporting C libraries.
	 */
	get version() {
		return $.version;
	}
	/**
	 * Signals for the nx.js application process to exit. The "exit" event will be invoked once the event loop is stopped.
	 */
	exit!: () => never;

	/**
	 * @private
	 */
	constructor() {
		super();
		this.env = new Env();
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
			!internal.keyboardInitialized &&
			(type === 'keydown' || type === 'keyup')
		) {
			$.hidInitializeKeyboard();
			internal.keyboardInitialized = true;
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
	 * Returns the current working directory as a URL string with a trailing slash.
	 *
	 * @example "sdmc:/switch/"
	 */
	cwd() {
		return `${$.cwd()}/`;
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
		return $.chdir(pathToString(dir));
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

	readFile = readFile;
	readDirSync = readDirSync;
	readFileSync = readFileSync;
	writeFileSync = writeFileSync;
	remove = remove;
	stat = stat;

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
		if (!internal.nifmInitialized) {
			addEventListener('unload', $.nifmInitialize());
			internal.nifmInitialized = true;
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
		if (!internal.vibrationDevicesInitialized) {
			$.hidInitializeVibrationDevices();
			$.hidSendVibrationValues(DEFAULT_VIBRATION);
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
		let next = internal.vibrationPattern?.shift();
		if (typeof next === 'undefined') {
			// Pattern completed
			next = 0;
		}
		if (typeof next === 'number') {
			// Pause interval
			$.hidSendVibrationValues(STOP_VIBRATION);
			if (next > 0) {
				internal.vibrationTimeoutId = setTimeout(
					this.#processVibrations,
					next
				);
			}
		} else {
			// Vibration interval
			$.hidSendVibrationValues(next);
			internal.vibrationTimeoutId = setTimeout(
				this.#processVibrations,
				next.duration
			);
		}
	};

	inspect = inspect;
}
