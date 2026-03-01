import { $ } from './$';
import { ErrorEvent, Event } from './polyfills/event';
import { EventTarget } from './polyfills/event-target';
import { stub, toPromise } from './utils';

const encoder = new TextEncoder();

/**
 * Options for creating a datagram socket.
 *
 * @see {@link listenDatagram | `Switch.listenDatagram()`}
 */
export interface DatagramOptions {
	/**
	 * The port number to bind to. Pass `0` or omit for a random port.
	 *
	 * @default 0
	 */
	port?: number;
	/**
	 * The IP address of the network interface to bind to.
	 *
	 * @default "0.0.0.0"
	 */
	ip?: string;
	/**
	 * Handler invoked when a datagram is received.
	 *
	 * This is a shorthand for:
	 * ```js
	 * socket.addEventListener('message', handler);
	 * ```
	 */
	message?: (e: DatagramEvent) => void;
}

/**
 * Init data for a received datagram.
 */
export interface DatagramEventInit extends EventInit {
	/** The received data. */
	data: ArrayBuffer;
	/** The sender's IP address. */
	remoteAddress: string;
	/** The sender's port number. */
	remotePort: number;
}

/**
 * Event dispatched when a datagram is received on a {@link DatagramSocket | `DatagramSocket`}.
 */
export class DatagramEvent extends Event {
	/** The received data. */
	readonly data: ArrayBuffer;
	/** The sender's IP address. */
	readonly remoteAddress: string;
	/** The sender's port number. */
	readonly remotePort: number;

	constructor(type: string, init: DatagramEventInit) {
		super(type, init);
		this.data = init.data;
		this.remoteAddress = init.remoteAddress;
		this.remotePort = init.remotePort;
	}
}

/**
 * A UDP datagram socket bound to a local address and port.
 *
 * Uses the Web `EventTarget` API. Dispatches {@link DatagramEvent | `DatagramEvent`}
 * instances when datagrams are received.
 *
 * @example
 * ```typescript
 * const socket = Switch.listenDatagram({ port: 9999 });
 *
 * socket.addEventListener('message', (e) => {
 *   console.log(`From ${e.remoteAddress}:${e.remotePort}:`, e.data);
 *   socket.send('pong', e.remoteAddress, e.remotePort);
 * });
 * ```
 */
export class DatagramSocket extends EventTarget {
	/**
	 * The file descriptor of the underlying socket.
	 * Set by the native `$.udpInit` via a getter on the prototype.
	 * @internal
	 */
	declare readonly fd: number;

	/**
	 * The local address information of the bound socket.
	 * Set by the native `$.udpInit` via a getter on the prototype.
	 * @internal
	 */
	declare readonly address: { address: string; port: number };

	/**
	 * Send a datagram to the specified address and port.
	 *
	 * @param data The data to send. Strings are encoded as UTF-8.
	 * @param remoteAddress The destination IP address.
	 * @param remotePort The destination port number.
	 */
	async send(
		data: Uint8Array | ArrayBuffer | string,
		remoteAddress: string,
		remotePort: number,
	): Promise<number> {
		let buf: ArrayBuffer;
		if (typeof data === 'string') {
			buf = encoder.encode(data).buffer;
		} else if (data instanceof Uint8Array) {
			buf = data.buffer.slice(
				data.byteOffset,
				data.byteOffset + data.byteLength,
			);
		} else {
			buf = data;
		}
		return toPromise($.udpSend, this.fd, buf, remoteAddress, remotePort);
	}

	/**
	 * Enable or disable broadcast on this socket.
	 *
	 * @param enabled Whether to enable broadcast.
	 */
	setBroadcast(enabled: boolean): void {
		stub();
	}

	/**
	 * Join a multicast group.
	 *
	 * @param multicastAddress The multicast group address to join (e.g. `"239.255.255.250"`).
	 * @param multicastInterface The local interface address to use. Defaults to `"0.0.0.0"` (any interface).
	 */
	addMembership(multicastAddress: string, multicastInterface?: string): void {
		stub();
	}

	/**
	 * Leave a multicast group.
	 *
	 * @param multicastAddress The multicast group address to leave.
	 * @param multicastInterface The local interface address. Defaults to `"0.0.0.0"`.
	 */
	dropMembership(multicastAddress: string, multicastInterface?: string): void {
		stub();
	}

	/**
	 * Close the socket. No more datagrams will be received or sent.
	 */
	close(): void {
		stub();
	}
}

// Register native methods on the DatagramSocket prototype
$.udpInit(DatagramSocket);

/**
 * Creates a UDP datagram socket bound to the specified port.
 *
 * @param opts Options for the datagram socket.
 * @returns The bound datagram socket.
 *
 * @example
 * ```typescript
 * const socket = Switch.listenDatagram({
 *   port: 9999,
 *   message(e) {
 *     console.log(`From ${e.remoteAddress}:${e.remotePort}`);
 *     socket.send('pong', e.remoteAddress, e.remotePort);
 *   },
 * });
 *
 * // Send a broadcast
 * socket.setBroadcast(true);
 * socket.send('hello', '255.255.255.255', 9999);
 * ```
 */
export function listenDatagram(opts: DatagramOptions): DatagramSocket {
	const { ip = '0.0.0.0', port = 0, message } = opts;

	const nativeSocket = $.udpNew(ip, port, (err, data, remoteIp, remotePort) => {
		if (err) {
			socket.dispatchEvent(new ErrorEvent('error', { error: err }));
			return;
		}
		socket.dispatchEvent(
			new DatagramEvent('message', {
				data: data!,
				remoteAddress: remoteIp!,
				remotePort: remotePort!,
			}),
		);
	});

	// The native $.udpNew returns a C opaque object with the prototype
	// methods already set. Set its prototype to DatagramSocket.prototype
	// so it's a proper DatagramSocket instance.
	Object.setPrototypeOf(nativeSocket, DatagramSocket.prototype);
	const socket = nativeSocket as unknown as DatagramSocket;

	if (message) {
		socket.addEventListener('message', message);
	}

	return socket;
}
