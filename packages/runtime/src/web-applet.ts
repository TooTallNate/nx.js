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

export interface WebAppletShowResult {
	exitReason?: number;
	lastUrl?: string;
}

/**
 * Opens the Switch's built-in web browser as a Library Applet.
 * Requires Application mode (NSP install or hbmenu via title override).
 *
 * Supports two URL schemes:
 * - `https://` or `http://` — opens the online web browser
 * - `romfs:/` — opens the offline browser, serving HTML from the app's romfs
 *
 * @example Online (blocking)
 * ```typescript
 * const applet = new Switch.WebApplet('https://example.com');
 * const result = await applet.show();
 * console.log(result.lastUrl);
 * ```
 *
 * @example Offline from romfs (blocking)
 * ```typescript
 * const applet = new Switch.WebApplet('romfs:/index.html');
 * const result = await applet.show();
 * ```
 *
 * @example Session mode with messaging (non-blocking)
 * ```typescript
 * const applet = new Switch.WebApplet('romfs:/app.html');
 * applet.jsExtension = true;
 *
 * applet.addEventListener('message', (e) => {
 *   console.log('From browser:', e.data);
 *   applet.sendMessage('response');
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

	/**
	 * Enable the `window.nx` JavaScript API in the browser for
	 * bidirectional messaging. Only useful with {@link start} (session mode).
	 */
	get jsExtension() {
		return this.#jsExtension;
	}

	set jsExtension(value: boolean) {
		this.#jsExtension = value;
	}

	/**
	 * Start the browser hidden (can be shown later with {@link appear}).
	 */
	get bootHidden() {
		return this.#bootHidden;
	}

	set bootHidden(value: boolean) {
		this.#bootHidden = value;
	}

	/**
	 * Whether the browser applet is currently running (session mode only).
	 */
	get running() {
		if (!this.#started) return false;
		return $.webAppletIsRunning(this.#native) as boolean;
	}

	/**
	 * Opens the browser and blocks until the user closes it.
	 * Returns information about why the browser was closed and the last URL visited.
	 */
	async show(): Promise<WebAppletShowResult> {
		this.#configure();
		return $.webAppletShow(this.#native) as WebAppletShowResult;
	}

	/**
	 * Opens the browser in non-blocking session mode.
	 * Use with `jsExtension = true` for bidirectional messaging via `window.nx`.
	 * Listen for `'message'` and `'exit'` events.
	 */
	async start(): Promise<void> {
		if (this.#started) {
			throw new Error('WebApplet already started');
		}

		this.#configure();
		$.webAppletStart(this.#native);
		this.#started = true;

		this.#pollInterval = setInterval(() => {
			if (!this.#started) return;

			const messages = $.webAppletPollMessages(
				this.#native,
			) as string[];
			for (const msg of messages) {
				this.dispatchEvent(new WebAppletMessageEvent(msg));
			}

			if (!$.webAppletIsRunning(this.#native)) {
				this.#cleanup();
				this.dispatchEvent(new Event('exit'));
			}
		}, 16);
	}

	/**
	 * Make the browser visible if it was started hidden.
	 */
	appear(): void {
		if (!this.#started) {
			throw new Error('WebApplet not started');
		}
		$.webAppletAppear(this.#native);
	}

	/**
	 * Send a string message to the browser page.
	 * The page receives it via `window.nx.onMessage`.
	 * Requires `jsExtension = true`.
	 */
	sendMessage(msg: string): boolean {
		if (!this.#started) {
			throw new Error('WebApplet not started');
		}
		return $.webAppletSendMessage(this.#native, msg) as boolean;
	}

	/**
	 * Request the browser to close gracefully.
	 */
	requestExit(): void {
		if (!this.#started) {
			throw new Error('WebApplet not started');
		}
		$.webAppletRequestExit(this.#native);
	}

	/**
	 * Force close the browser and clean up.
	 */
	close(): void {
		if (this.#started) {
			$.webAppletClose(this.#native);
			this.#cleanup();
		}
	}

	#configure() {
		$.webAppletSetUrl(this.#native, this.#url);
		$.webAppletSetJsExtension(this.#native, this.#jsExtension);
		if (this.#bootHidden) {
			$.webAppletSetBootMode(this.#native, 1);
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
