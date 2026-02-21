import { $ } from './$';
import { def } from './utils';
import { setInterval, clearInterval } from './timers';
import { EventTarget } from './polyfills/event-target';
import { Event } from './polyfills/event';

class WebAppletMessageEvent extends Event {
	data: string;
	constructor(data: string) {
		super('message');
		this.data = data;
	}
}

/**
 * Opens the Switch's built-in web browser as a Library Applet.
 * Requires firmware 7.0.0+.
 *
 * When `jsExtension` is enabled, the browser page gets a `window.nx` object
 * for bidirectional communication with the nx.js app.
 *
 * @example
 * ```typescript
 * const applet = new Switch.WebApplet('http://localhost:8080');
 * applet.jsExtension = true;
 *
 * applet.addEventListener('message', (e) => {
 *   console.log('From browser:', e.data);
 *   applet.sendMessage('pong');
 * });
 *
 * applet.addEventListener('exit', () => {
 *   console.log('Browser closed');
 * });
 *
 * await applet.start();
 * ```
 */
export class WebApplet extends EventTarget {
	#native: any;
	#pollInterval: any = null;
	#started = false;
	#url: string;
	#jsExtension = false;
	#bootHidden = false;

	constructor(url: string) {
		super();
		this.#url = url;
		this.#native = $.webAppletNew();
	}

	get jsExtension() {
		return this.#jsExtension;
	}

	set jsExtension(value: boolean) {
		this.#jsExtension = value;
	}

	get bootHidden() {
		return this.#bootHidden;
	}

	set bootHidden(value: boolean) {
		this.#bootHidden = value;
	}

	get running() {
		if (!this.#started) return false;
		return $.webAppletIsRunning(this.#native);
	}

	async start(): Promise<void> {
		if (this.#started) {
			throw new Error('WebApplet already started');
		}

		$.webAppletSetUrl(this.#native, this.#url);
		$.webAppletSetJsExtension(this.#native, this.#jsExtension);
		if (this.#bootHidden) {
			$.webAppletSetBootMode(this.#native, 1);
		}

		$.webAppletStart(this.#native);
		this.#started = true;

		this.#pollInterval = setInterval(() => {
			if (!this.#started) return;

			const messages = $.webAppletPollMessages(this.#native);
			for (const msg of messages) {
				this.dispatchEvent(new WebAppletMessageEvent(msg));
			}

			if (!$.webAppletIsRunning(this.#native)) {
				this.#cleanup();
				this.dispatchEvent(new Event('exit'));
			}
		}, 16);
	}

	appear(): void {
		if (!this.#started) {
			throw new Error('WebApplet not started');
		}
		$.webAppletAppear(this.#native);
	}

	sendMessage(msg: string): boolean {
		if (!this.#started) {
			throw new Error('WebApplet not started');
		}
		return $.webAppletSendMessage(this.#native, msg);
	}

	requestExit(): void {
		if (!this.#started) {
			throw new Error('WebApplet not started');
		}
		$.webAppletRequestExit(this.#native);
	}

	close(): void {
		if (this.#started) {
			$.webAppletClose(this.#native);
			this.#cleanup();
		}
	}

	#cleanup() {
		this.#started = false;
		if (this.#pollInterval !== null) {
			clearInterval(this.#pollInterval);
			this.#pollInterval = null;
		}
	}
}

def(WebApplet);
