import { $ } from './$';
import { INTERNAL_SYMBOL } from './internal';
import { Env } from './env';
import { Event } from './polyfills/event';
import { pathToString } from './utils';
import {
	Socket,
	connect as tcpConnect,
	createServer,
	parseAddress,
	Server,
} from './tcp';

export * from './dns';
export * from './env';
export * from './fs';
export * from './inspect';
export * from './switch/nifm';
export * from './switch/ns';
export * from './switch/irsensor';
export { Socket, Server };

export type PathLike = string | URL;

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

export interface Vibration {
	duration: number;
	lowAmp: number;
	lowFreq: number;
	highAmp: number;
	highFreq: number;
}

export interface Stats {
	size: number;
	mtime: number;
	atime: number;
	ctime: number;
	mode: number;
	uid: number;
	gid: number;
}

/**
 * Specifies the port number and optional IP address
 * for creating a TCP server.
 *
 * @see {@link listen | `Switch.listen()`}
 */
export interface ListenOptions {
	/**
	 * The IP address of the network interface to bind to.
	 *
	 * If not defined, defaults to `0.0.0.0` to allow
	 * connections on any network device.
	 *
	 * @example "127.0.0.1"
	 */
	ip?: string;
	/**
	 * The port number to accept TCP connection from.
	 *
	 * @example 80
	 */
	port: number;
	/**
	 * Function to invoke when a new TCP socket has connected.
	 *
	 * This is a shorthand for:
	 *
	 * ```js
	 * server.addEventListener('accept', fn);
	 * ```
	 */
	accept?: (e: SocketEvent) => void;
}

export interface NetworkInfo {
	ip: string;
	subnetMask: string;
	gateway: string;
}

/**
 * Specifies the port number and optional hostname for connecting
 * to a remove server over the network.
 *
 * @see {@link connect | `Switch.connect()`}
 */
export interface SocketAddress {
	/**
	 * The hostname of the destination server to connect to.
	 *
	 * If not defined, then `hostname` defaults to `127.0.0.1`.
	 *
	 * @example "example.com"
	 */
	hostname: string;
	/**
	 * The port number to connect to.
	 *
	 * @example 80
	 */
	port: number;
}

export type SecureTransportKind = 'off' | 'on' | 'starttls';

export interface SocketEventInit extends EventInit {
	socket: Socket;
}

export class SocketEvent extends Event {
	socket: Socket;
	constructor(type: string, init: SocketEventInit) {
		super(type, init);
		this.socket = init.socket;
	}
}

export interface SocketOptions {
	/**
	 * Specifies whether or not to use TLS when creating the TCP socket.
	 *  - `off` — Do not use TLS.
	 *  - `on` — Use TLS.
	 *  - `starttls` — Do not use TLS initially, but allow the socket to be upgraded to use TLS by calling {@link Socket.startTls | `startTls()`}.
	 */
	secureTransport?: SecureTransportKind;
	/**
	 * Defines whether the writable side of the TCP socket will automatically close on end-of-file (EOF).
	 * When set to false, the writable side of the TCP socket will automatically close on EOF.
	 * When set to true, the writable side of the TCP socket will remain open on EOF.
	 * This option is similar to that offered by the Node.js net module and allows interoperability with code which utilizes it.
	 */
	allowHalfOpen?: boolean;
}

export interface SocketInfo {
	remoteAddress: string;
	localAddress: string;
}

/**
 * A Map-like object providing methods to interact with the environment variables of the process.
 */
export const env = new Env();

/**
 * Array of the arguments passed to the process. Under normal circumstances, this array contains a single entry with the absolute path to the `.nro` file.
 * @example [ "sdmc:/switch/nxjs.nro" ]
 */
export const argv = $.argv;

/**
 * String value of the entrypoint JavaScript file that was evaluated. If a `main.js` file is present on the application's RomFS, then that will be executed first, in which case the value will be `romfs:/main.js`. Otherwise, the value will be the path of the `.nro` file on the SD card, with the `.nro` extension replaced with `.js`.
 * @example "romfs:/main.js"
 * @example "sdmc:/switch/nxjs.js"
 */
export const entrypoint = $.entrypoint;

/**
 * An Object containing the versions numbers of nx.js and all supporting C libraries.
 */
export const version = $.version;

/**
 * Signals for the nx.js application process to exit. The "exit" event will be invoked once the event loop is stopped.
 */
export function exit(): never {
	$.exit();
}

/**
 * Returns the current working directory as a URL string with a trailing slash.
 *
 * @example "sdmc:/switch/"
 */
export function cwd() {
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
export function chdir(dir: PathLike) {
	$.chdir(pathToString(dir));
}

/**
 * Creates a TCP connection to the specified `address`.
 *
 * @param address Hostname and port number of the destination TCP server to connect to.
 * @param opts Optional
 * @see https://sockets-api.proposal.wintercg.org
 */
export function connect<Host extends string, Port extends string>(
	address: `${Host}:${Port}` | SocketAddress,
	opts?: SocketOptions
) {
	return new Socket(
		// @ts-expect-error Internal constructor
		INTERNAL_SYMBOL,
		typeof address === 'string' ? parseAddress(address) : address,
		{ ...opts, connect: tcpConnect }
	);
}

export function listen(opts: ListenOptions) {
	const { ip = '0.0.0.0', port, accept } = opts;
	const server = createServer(ip, port);
	if (accept) {
		server.addEventListener('accept', accept);
	}
	return server;
}
