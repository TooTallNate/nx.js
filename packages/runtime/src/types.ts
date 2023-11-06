import type { SwitchClass } from './switch';
import type { Socket } from './tcp';

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
