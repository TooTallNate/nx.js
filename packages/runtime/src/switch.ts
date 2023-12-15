import { $ } from './$';
import { INTERNAL_SYMBOL, type Keys } from './internal';
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
	nifmInitialized?: boolean;
}

interface SwitchEventHandlersEventMap {
	buttondown: UIEvent;
	buttonup: UIEvent;
	keydown: KeyboardEvent;
	keyup: KeyboardEvent;
}

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

	inspect = inspect;
}
