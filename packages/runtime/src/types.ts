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
	secureTransport?: SecureTransportKind;
	allowHalfOpen?: boolean;
}

export interface SocketInfo {
	remoteAddress: string;
	localAddress: string;
}
