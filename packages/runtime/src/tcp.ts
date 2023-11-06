import { $ } from './$';
import { encoder } from './polyfills/text-encoder';
import { SocketEvent } from './polyfills/event';
import { resolve } from './dns';
import {
	Deferred,
	assertInternalConstructor,
	bufferSourceToArrayBuffer,
	def,
	toPromise,
} from './utils';
import type {
	BufferSource,
	SecureTransportKind,
	SocketAddress,
	SocketInfo,
} from './types';
import { INTERNAL_SYMBOL, type SocketOptionsInternal } from './internal';

interface SocketInternal {
	fd: number;
	opened: Deferred<SocketInfo>;
	closed: Deferred<void>;
	secureTransport: SecureTransportKind;
	allowHalfOpen: boolean;
}

const socketInternal = new WeakMap<Socket, SocketInternal>();

export function parseAddress(address: string): SocketAddress {
	const firstColon = address.indexOf(':');
	return {
		hostname: address.slice(0, firstColon),
		port: +address.slice(firstColon + 1),
	};
}

export function read(fd: number, buffer: BufferSource) {
	const ab = bufferSourceToArrayBuffer(buffer);
	return toPromise($.read, fd, ab);
}

export function write(fd: number, data: string | BufferSource) {
	const d = typeof data === 'string' ? encoder.encode(data) : data;
	const ab = bufferSourceToArrayBuffer(d);
	return toPromise($.write, fd, ab);
}

/**
 * Creates a TCP connection specified by the `hostname`
 * and `port`.
 *
 * @param opts Object containing the `port` number and `hostname` (defaults to `127.0.0.1`) to connect to.
 * @returns Promise that is fulfilled once the connection has been successfully established.
 */
export async function connect(opts: SocketAddress) {
	const { hostname = '127.0.0.1', port } = opts;
	const [ip] = await resolve(hostname);
	if (!ip) {
		throw new Error(`Could not resolve "${hostname}" to an IP address`);
	}
	return toPromise($.connect, ip, port);
}

/**
 * The `Socket` class represents a TCP connection, from which you can
 * read and write data. A socket begins in a _connected_ state (if the
 * socket fails to connect, an error is thrown). While in a _connected_
 * state, the socket’s `ReadableStream` and `WritableStream` can be
 * read from and written to respectively.
 */
export class Socket {
	readonly readable: ReadableStream<Uint8Array>;
	readonly writable: WritableStream<Uint8Array>;

	readonly opened: Promise<SocketInfo>;
	readonly closed: Promise<void>;

	/**
	 * @ignore
	 */
	constructor() {
		const address: SocketAddress = arguments[1];
		const {
			secureTransport = 'off',
			allowHalfOpen = false,
			connect,
		}: SocketOptionsInternal = arguments[2] || {};
		assertInternalConstructor(arguments);
		const socket = this;
		const i: SocketInternal = {
			fd: -1,
			opened: new Deferred(),
			closed: new Deferred(),
			secureTransport,
			allowHalfOpen,
		};
		socketInternal.set(this, i);
		this.opened = i.opened.promise;
		this.closed = i.closed.promise;

		const readBuffer = new ArrayBuffer(64 * 1024);
		this.readable = new ReadableStream({
			async pull(controller) {
				if (i.opened.pending) {
					await socket.opened;
				}
				const bytesRead = await read(i.fd, readBuffer);
				if (bytesRead === 0) {
					controller.close();
					if (!allowHalfOpen) {
						socket.close();
					}
					return;
				}
				controller.enqueue(new Uint8Array(readBuffer, 0, bytesRead));
			},
		});
		this.writable = new WritableStream({
			async write(chunk, controller) {
				if (i.opened.pending) {
					await socket.opened;
				}
				await write(i.fd, chunk);
			},
		});

		connect(address)
			.then((fd) => {
				i.fd = fd;
				i.opened.resolve({
					localAddress: '',
					remoteAddress: '',
				});
			})
			.catch((err) => {
				this.close(err);
			});
	}

	/**
	 * Closes the socket and its underlying connection.
	 */
	close(reason?: any) {
		if (!this.readable.locked) {
			this.readable.cancel(reason);
		}
		if (!this.writable.locked) {
			this.writable.abort(reason);
		}
		const i = socketInternal.get(this);
		if (i) {
			if (i.opened.pending) {
				i.opened.reject(reason);
			}
			if (i.fd !== -1) {
				$.close(i.fd);
				i.fd = -1;
			}
		}
		return this.closed;
	}

	/**
	 * Enables opportunistic TLS (otherwise known as
	 * {@link https://en.wikipedia.org/wiki/Opportunistic_TLS | StartTLS})
	 * which is a requirement for some protocols
	 * (primarily postgres/mysql and other DB protocols).
	 */
	startTls(): Socket {
		throw new Error('Method not implemented.');
	}
}
def('Socket', Socket);

export class Server extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
	}

	/**
	 * The "accept" event is fired when a new TCP client connection
	 * has been established. Use the `fd` property to determine
	 * which file descriptor to read / write to interact with this
	 * socket.
	 */
	// @ts-expect-error Not sure why this is error :(
	addEventListener(
		type: 'accept',
		listener: (ev: SocketEvent) => any,
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
		super.addEventListener(type, callback, options);
	}

	/**
	 * Shuts down the server and closes any existing client connections.
	 */
	close() {}
}
$.tcpInitServer(Server);
def('Server', Server);

export function createServer(ip: string, port: number) {
	const server = $.tcpServerNew(ip, port, function onAccept(fd) {
		// @ts-expect-error Internal constructor
		const socket = new Socket(INTERNAL_SYMBOL, null, {
			async connect() {
				return fd;
			},
		});
		server.dispatchEvent(new SocketEvent('accept', { socket }));
	});
	Object.setPrototypeOf(server, Server.prototype);
	EventTarget.call(server);
	return server;
}
