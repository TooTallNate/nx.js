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
	/** The version of the [ada](https://github.com/ada-url/ada) URL parsing library. */
	readonly ada: string;
	/**
	 * The version of the [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere)
	 * custom firmware running on the Switch, or `undefined` if not running
	 * Atmosphère.
	 */
	readonly ams: string | undefined;
	/**
	 * `true` if the Switch is running Atmosphère from emuMMC, `false` if
	 * running sysMMC, or `undefined` if not running Atmosphère.
	 */
	readonly emummc: boolean | undefined;
	/** The version of the [FreeType 2](https://freetype.org/) font rendering library. */
	readonly freetype2: string;
	/** The version of the [HarfBuzz](https://harfbuzz.github.io/) text shaping library. */
	readonly harfbuzz: string;
	/** The Horizon OS (Switch system software) version (e.g. `"18.1.0"`). */
	readonly hos: string;
	/** The version of [libnx](https://github.com/switchbrew/libnx) the runtime was built against. */
	readonly libnx: string;
	/** The version of [mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/) used for TLS / cryptography. */
	readonly mbedtls: string;
	/** The semver version of the nx.js runtime itself. */
	readonly nxjs: string;
	/** The version of the [Skia](https://skia.org/) 2D graphics library used for canvas rendering (milestone number, e.g. `"149"`). */
	readonly skia: string;
	/** The version of [libpng](http://www.libpng.org/pub/png/libpng.html) used for PNG decoding. */
	readonly png: string;
	/** The version of [libjpeg-turbo](https://libjpeg-turbo.org/) used for JPEG decoding. */
	readonly turbojpeg: string;
	/** The version of the [V8](https://v8.dev/) JavaScript engine. */
	readonly v8: string;
	/** The version of [libwebp](https://developers.google.com/speed/webp/) used for WebP decoding. */
	readonly webp: string;
	/** The version of [zlib](https://zlib.net/) used for deflate / gzip compression. */
	readonly zlib: string;
	/** The version of [Zstandard](https://github.com/facebook/zstd) (zstd) used for zstd compression. */
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

/**
 * Memory usage statistics from the [V8](https://v8.dev/) JavaScript engine,
 * as reported by V8's `HeapStatistics`.
 *
 * @see {@link memoryUsage}
 */
export interface MemoryUsage {
	/** Total size of the V8 heap, in bytes. */
	totalHeapSize: number;
	/** Total size of executable (JIT code) portions of the heap, in bytes. */
	totalHeapSizeExecutable: number;
	/** Committed physical size of the heap, in bytes. */
	totalPhysicalSize: number;
	/** Total memory available to the heap before hitting the limit, in bytes. */
	totalAvailableSize: number;
	/** Memory currently used by live objects on the heap, in bytes. */
	usedHeapSize: number;
	/** Maximum size the heap is allowed to grow to, in bytes. */
	heapSizeLimit: number;
	/** Memory currently allocated via the engine's `malloc`, in bytes. */
	mallocedMemory: number;
	/** Peak memory ever allocated via the engine's `malloc`, in bytes. */
	peakMallocedMemory: number;
	/** Number of active (native) contexts. */
	numberOfNativeContexts: number;
	/** Number of detached (pending GC) contexts. */
	numberOfDetachedContexts: number;
	/**
	 * Bytes of off-heap memory (e.g. `ArrayBuffer` backing stores) that V8 is
	 * keeping alive. A large value with a small {@link MemoryUsage.usedHeapSize | `usedHeapSize`}
	 * means native memory is held by not-yet-collected JS objects.
	 */
	externalMemory: number;
	/**
	 * Total native (newlib) heap capacity of the process, in bytes. Every
	 * native allocation — the V8 heap, JIT code arena, render surfaces, worker
	 * thread stacks, `ArrayBuffer` backings — competes within this budget.
	 */
	nativeHeapTotal: number;
	/** Current size of the native allocator's arena, in bytes. */
	nativeHeapArena: number;
	/** Bytes currently allocated from the native heap. */
	nativeHeapUsed: number;
	/** Bytes free within the native allocator's current arena. */
	nativeHeapFree: number;
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
 * String value of the entrypoint JavaScript file that was evaluated. It is the
 * base URL used to resolve relative `fetch()` / `Image` / `Audio` paths.
 *
 * It is resolved in the following order:
 *
 *  - If a second launch argument ({@link argv | `Switch.argv[1]`}) was provided
 *    (a "bootstrap" launch, where a thin launcher hands the nx.js runtime an app
 *    to run):
 *    - a `.nro` path: its embedded RomFS is mounted and the value is
 *      `"romfs:/main.js"`.
 *    - any other path (typically a `.js` file): the value is that path as given.
 *  - Otherwise (a standalone app): if a `main.js` is present in the
 *    application's own RomFS, the value is `"romfs:/main.js"`. Failing that, it
 *    is the path of the `.nro` on the SD card with `.nro` replaced by `.js`.
 *
 * Note: the nx.js runtime's own files (e.g. its source map) are mounted
 * separately under `nxjs:` so that `romfs:` always refers to the running app.
 * @example "romfs:/main.js"
 * @example "sdmc:/switch/my-app.js"
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
 * Returns memory usage statistics from the [V8](https://v8.dev/) JavaScript
 * engine via V8's `HeapStatistics`.
 *
 * Useful for debugging memory leaks and monitoring memory pressure.
 *
 * @example
 *
 * ```typescript
 * const mem = Switch.memoryUsage();
 * console.log(`Heap used: ${mem.usedHeapSize} bytes`);
 * console.log(`Heap total: ${mem.totalHeapSize} bytes`);
 * console.log(`Heap limit: ${mem.heapSizeLimit} bytes`);
 * ```
 */
export function memoryUsage(): MemoryUsage {
	return $.memoryUsage();
}
