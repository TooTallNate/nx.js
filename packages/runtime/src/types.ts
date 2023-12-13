import type { Image } from './image';
import type { Screen } from './screen';
import type { OffscreenCanvas } from './canvas/offscreen-canvas';
import type { SwitchClass } from './switch';
import type { Socket } from './tcp';

export type DOMHighResTimeStamp = number;

export type PathLike = string | URL;

export interface Stats {
	size: number;
	mtime: number;
	atime: number;
	ctime: number;
	mode: number;
	uid: number;
	gid: number;
}

export interface ArrayBufferView {
	/**
	 * The ArrayBuffer instance referenced by the array.
	 */
	buffer: ArrayBuffer;

	/**
	 * The length in bytes of the array.
	 */
	byteLength: number;

	/**
	 * The offset in bytes of the array.
	 */
	byteOffset: number;
}

export type BufferSource = ArrayBufferView | ArrayBuffer;

export interface ConnectOpts {
	hostname?: string;
	port: number;
}

/**
 * Specifies the port number and optional IP address
 * for creating a TCP server.
 *
 * {@link SwitchClass.listen}
 */
export interface ListenOpts {
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
 * {@link SwitchClass.connect}
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

export interface ImageEncodeOptions {
	quality?: number;
	type?: string;
}

export type CanvasFillRule = 'evenodd' | 'nonzero';
export type CanvasImageSource = Image | Screen | OffscreenCanvas;
export type CanvasLineCap = 'butt' | 'round' | 'square';
export type CanvasLineJoin = 'bevel' | 'miter' | 'round';
export type CanvasTextAlign = 'center' | 'end' | 'left' | 'right' | 'start';
export type GlobalCompositeOperation =
	| 'color'
	| 'color-burn'
	| 'color-dodge'
	| 'copy'
	| 'darken'
	| 'destination-atop'
	| 'destination-in'
	| 'destination-out'
	| 'destination-over'
	| 'difference'
	| 'exclusion'
	| 'hard-light'
	| 'hue'
	| 'lighten'
	| 'lighter'
	| 'luminosity'
	| 'multiply'
	| 'overlay'
	| 'saturate'
	| 'saturation'
	| 'screen'
	| 'soft-light'
	| 'source-atop'
	| 'source-in'
	| 'source-out'
	| 'source-over'
	| 'xor';
export type ImageSmoothingQuality = 'high' | 'low' | 'medium';

export type FontDisplay = 'auto' | 'block' | 'fallback' | 'optional' | 'swap';
export type FontFaceLoadStatus = 'error' | 'loaded' | 'loading' | 'unloaded';
export type FontFaceSetLoadStatus = 'loaded' | 'loading';
