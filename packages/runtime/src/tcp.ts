import { $ } from './$';
import { SocketEvent } from './polyfills/event';
import { assertInternalConstructor } from './utils';

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

export function createServer(ip: string, port: number) {
	const server = $.tcpServerNew(ip, port, function onAccept(fd) {
		server.dispatchEvent(new SocketEvent('accept', { fd }));
	});
	Object.setPrototypeOf(server, Server.prototype);
	EventTarget.call(server);
	return server;
}
