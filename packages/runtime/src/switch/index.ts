import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { Event } from '../polyfills/event';
import {
	createServer,
	parseAddress,
	Server,
	Socket,
	connect as tcpConnect,
} from '../tcp';
import { pathToString } from '../utils';
import { Env } from './env';

export * from '../fs';
export type { DatagramEventInit, DatagramOptions } from '../udp';
export {
	DatagramEvent,
	DatagramSocket,
	listenDatagram,
} from '../udp';
export * from './album';
export * from './dns';
export * from './env';
export * from './file-system';
export * from './inspect';
export * from './irsensor';
export * from './nifm';
export * from './ns';
export * from './profile';
export * from './savedata';
export * from './service';
export { Socket, Server };
export { WebApplet, type WebAppletOptions } from '../web-applet';

export type PathLike = string | URL;

export interface Versions {
	/**
	 * The version of the ada URL parsing library.
	 */
	readonly ada: string;
	/**
	 * The version of the Atmosphère custom firmware running on the Switch, or `undefined` if not running Atmosphère.
	 */
	readonly ams: string | undefined;
	readonly cairo: string;
	/**
	 * `true` if the Switch is running Atmosphère from emuMMC, `false` if running sysMMC, or `undefined` if not running Atmosphère.
	 */
	readonly emummc: boolean | undefined;
	readonly freetype2: string;
	readonly harfbuzz: string;
	readonly hos: string;
	readonly libnx: string;
	readonly mbedtls: string;
	readonly nxjs: string;
	readonly pixman: string;
	readonly png: string;
	readonly quickjs: string;
	readonly turbojpeg: string;
	readonly wasm3: string;
	readonly webp: string;
	readonly zlib: string;
	readonly zstd: string;
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

export interface MemoryUsage {
	/**
	 * Total memory currently allocated by the C heap allocator (`malloc`).
	 * This includes all native allocations (libraries, buffers, etc.) and
	 * the QuickJS engine's allocations.
	 * Similar to Node.js `rss` (Resident Set Size).
	 */
	rss: number;
	/**
	 * Total memory footprint of the QuickJS JavaScript engine,
	 * including internal bookkeeping overhead.
	 * Similar to Node.js `heapTotal`.
	 */
	heapTotal: number;
	/**
	 * Memory allocated by the QuickJS JavaScript engine for
	 * JS objects, strings, and other runtime data.
	 * Similar to Node.js `heapUsed`.
	 */
	heapUsed: number;
	/**
	 * Total memory budget available to the process, as reported by the Switch kernel
	 * via `svcGetInfo(InfoType_TotalMemorySize)`.
	 *
	 * This value depends on the applet type:
	 * - In "full-memory mode" (Application): ~3.5 GB
	 * - In "applet mode" (LibraryApplet): ~400 MB
	 *
	 * @see {@link appletType | `Switch.appletType()`}
	 */
	totalSystemMemory: number;
	/**
	 * Total memory currently mapped by the process, as reported by the Switch kernel
	 * via `svcGetInfo(InfoType_UsedMemorySize)`. This includes all virtual memory
	 * pages mapped by the process (code, stack, heap region, etc.).
	 */
	usedSystemMemory: number;
	/**
	 * Total memory available for the process to allocate. This accounts for both
	 * free space in the C allocator's free list and unmapped heap space that
	 * has not yet been claimed by the allocator.
	 *
	 * This is the same value returned by {@link availableMemory | `Switch.availableMemory()`}.
	 */
	availableMemory: number;
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
	/**
	 * When `true` (default), the TLS handshake will verify the server's
	 * certificate against the system CA certificate store. Set to `false`
	 * to disable certificate verification (e.g. for development/testing
	 * with self-signed certificates).
	 *
	 * Only applicable when `secureTransport` is `'on'` or `'starttls'`.
	 *
	 * @default true
	 */
	rejectUnauthorized?: boolean;
}

export interface SocketInfo {
	remoteAddress: string;
	localAddress: string;
}

/**
 * A Map-like object providing methods to interact with the environment variables of the process.
 *
 * See {@link Env | `Switch.Env`} for more information.
 */
// @ts-expect-error Internal constructor
export const env = new Env(INTERNAL_SYMBOL);

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
 * Signals for the nx.js application to exit. The global "unload"
 * event will be invoked after the event loop has stopped.
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
	return $.cwd();
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
 * @param opts Socket options, for example to create a secure TLS connection.
 * @see https://sockets-api.proposal.wintercg.org
 */
export function connect<Host extends string, Port extends string>(
	address: `${Host}:${Port}` | SocketAddress,
	opts?: SocketOptions,
) {
	return new Socket(
		// @ts-expect-error Internal constructor
		INTERNAL_SYMBOL,
		typeof address === 'string' ? parseAddress(address) : address,
		{ ...opts, connect: tcpConnect },
	);
}

/**
 * Creates a TCP server bound to the specified `port` number.
 *
 * @param opts Object containing the port number and other configuration properties.
 */
export function listen(opts: ListenOptions) {
	const { ip = '0.0.0.0', port, accept } = opts;
	const server = createServer(ip, port);
	if (accept) {
		server.addEventListener('accept', accept);
	}
	return server;
}

/**
 * Returns the "applet type" of the running application.
 *
 * This can be used to differentiate between "applet mode" vs. "full-memory mode".
 *
 * @example
 *
 * ```typescript
 * import { AppletType } from '@nx.js/constants';
 *
 * if (Switch.appletType() === AppletType.Application) {
 *   console.log('Running in "full-memory mode"');
 * } else {
 *   console.log('Running in "applet mode"');
 * }
 * ```
 */
export function appletType(): number {
	return $.appletGetAppletType();
}

/**
 * Returns the current "operation mode" of the device.
 *
 * This can be used to identify if the device is handheld or docked.
 *
 * @example
 *
 * ```typescript
 * import { OperationMode } from '@nx.js/constants';
 *
 * if (Switch.operationMode() === OperationMode.Handheld) {
 *   console.log('Device is currently handheld');
 * } else {
 *   console.log('Device is currently docked');
 * }
 * ```
 */
export function operationMode(): number {
	return $.appletGetOperationMode();
}

/**
 * Set media playback state.
 *
 * @param state If `true`, screen dimming and auto sleep is disabled.
 */
export function setMediaPlaybackState(state: boolean) {
	$.appletSetMediaPlaybackState(state);
}

/**
 * Returns an object describing the memory usage of the nx.js process,
 * including both JavaScript heap statistics from QuickJS and system-level
 * memory information from the Switch kernel.
 *
 * Useful for debugging memory leaks and monitoring memory pressure.
 *
 * Similar to Node.js `process.memoryUsage()`.
 *
 * @example
 *
 * ```typescript
 * const mem = Switch.memoryUsage();
 * console.log(`Heap used: ${mem.heapUsed} bytes`);
 * console.log(`RSS: ${mem.rss} bytes`);
 * console.log(`System: ${mem.usedSystemMemory} / ${mem.totalSystemMemory} bytes`);
 * ```
 */
export function memoryUsage(): MemoryUsage {
	return $.memoryUsage();
}

/**
 * Returns the number of bytes of memory available for the process to allocate.
 * Calculated as the total process memory budget minus currently used memory,
 * as reported by the Switch kernel.
 *
 * Similar to Node.js `process.availableMemory()`.
 *
 * @example
 *
 * ```typescript
 * const available = Switch.availableMemory();
 * console.log(`Available memory: ${available} bytes`);
 * ```
 */
export function availableMemory(): number {
	return $.availableMemory();
}
