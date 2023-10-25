import { $ } from './$';
import { SocketEvent } from './polyfills/event';

export class Server extends EventTarget {
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
	 *
	 */
	close() {}
}
$.tcpInitServer(Server);

export function createServer(ip: string, port: number) {
	const server = $.tcpServerNew(ip, port, function onAccept(fd) {
		const event = new SocketEvent('accept', { fd });
		server.dispatchEvent(event);
	});
	Object.setPrototypeOf(server, Server.prototype);
	EventTarget.call(server);
	return server;
}
