import { $ } from './$';
import { resolveDns } from './dns';
import { EventTarget } from './polyfills/event-target';
import { SocketEvent, type SocketAddress, type SocketInfo } from './switch';
import {
	Deferred,
	assertInternalConstructor,
	bufferSourceToArrayBuffer,
	createInternal,
	toPromise,
} from './utils';
import type { BufferSource } from './types';
import {
	INTERNAL_SYMBOL,
	Opaque,
	type SocketOptionsInternal,
} from './internal';

export type TlsContextOpaque = Opaque<'TlsContext'>;

export function parseAddress(address: string): SocketAddress {
	const firstColon = address.indexOf(':');
	return {
		hostname: address.slice(0, firstColon),
		port: +address.slice(firstColon + 1),
	};
}

export async function connect(opts: SocketAddress) {
	const { hostname = '127.0.0.1', port } = opts;
	const [ip] = await resolveDns(hostname);
	if (!ip) {
		throw new Error(`Could not resolve "${hostname}" to an IP address`);
	}
	return toPromise($.connect, ip, port);
}

function read(fd: number, buffer: BufferSource) {
	const ab = bufferSourceToArrayBuffer(buffer);
	return toPromise($.read, fd, ab);
}

function write(fd: number, data: BufferSource) {
	const ab = bufferSourceToArrayBuffer(data);
	return toPromise($.write, fd, ab);
}

function tlsHandshake(fd: number, hostname: string) {
	return toPromise($.tlsHandshake, fd, hostname);
}

function tlsRead(ctx: TlsContextOpaque, buffer: BufferSource) {
	const ab = bufferSourceToArrayBuffer(buffer);
	return toPromise($.tlsRead, ctx, ab);
}

function tlsWrite(ctx: TlsContextOpaque, data: BufferSource) {
	const ab = bufferSourceToArrayBuffer(data);
	return toPromise($.tlsWrite, ctx, ab);
}

interface SocketInternal {
	fd: number;
	tls?: TlsContextOpaque;
	opened: Deferred<SocketInfo>;
	closed: Deferred<void>;
}

const _ = createInternal<Socket, SocketInternal>();

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
		};
		_.set(this, i);
		this.opened = i.opened.promise;
		this.closed = i.closed.promise;

		const readBuffer = new ArrayBuffer(64 * 1024);
		this.readable = new ReadableStream({
			async pull(controller) {
				if (i.opened.pending) {
					await socket.opened;
				}
				const bytesRead = await (i.tls
					? tlsRead(i.tls, readBuffer)
					: read(i.fd, readBuffer));
				if (bytesRead === 0) {
					controller.close();
					if (!allowHalfOpen) {
						socket.close();
					}
					return;
				}
				controller.enqueue(new Uint8Array(readBuffer.slice(0, bytesRead)));
			},
		});
		this.writable = new WritableStream({
			async write(chunk) {
				if (i.opened.pending) {
					await socket.opened;
				}
				await (i.tls ? tlsWrite(i.tls, chunk) : write(i.fd, chunk));
			},
			close() {
				socket.close();
			},
		});

		connect(address)
			.then((fd) => {
				i.fd = fd;
				if (secureTransport === 'on') {
					return tlsHandshake(fd, address.hostname);
				}
			})
			.then((tls) => {
				i.tls = tls;
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
		const i = _(this);
		if (i.opened.pending) {
			i.opened.reject(reason);
		}
		if (i.fd !== -1) {
			$.close(i.fd);
			i.fd = -1;
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
		options?: boolean | AddEventListenerOptions,
	): void;
	addEventListener(
		type: string,
		listener: EventListenerOrEventListenerObject,
		options?: boolean | AddEventListenerOptions,
	): void;
	addEventListener(
		type: string,
		callback: EventListenerOrEventListenerObject | null,
		options?: boolean | AddEventListenerOptions,
	): void {
		super.addEventListener(type, callback, options);
	}

	/**
	 * Shuts down the server and closes any existing client connections.
	 */
	close() {}
}
$.tcpServerInit(Server);

export function createServer(ip: string, port: number) {
	const server = $.tcpServerNew(ip, port, function onAccept(fd) {
		// @ts-expect-error Internal constructor
		const socket = new Socket(INTERNAL_SYMBOL, null, {
			allowHalfOpen: true,
			async connect() {
				return fd;
			},
		});
		server.dispatchEvent(new SocketEvent('accept', { socket }));
	});
	Object.setPrototypeOf(server, Server.prototype);
	return server;
}
