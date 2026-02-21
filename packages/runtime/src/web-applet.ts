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
 * Two modes of operation:
 *
 * ### Online mode (URL)
 * Opens the browser to a web URL:
 * ```typescript
 * const applet = new Switch.WebApplet('https://example.com');
 * const result = await applet.show(); // blocking
 * ```
 *
 * ### Offline mode (romfs HTML)
 * Serves HTML files from the app's romfs:
 * ```typescript
 * const applet = new Switch.WebApplet();
 * applet.documentPath = '/index.html';
 * const result = await applet.show();
 * ```
 *
 * ### Session mode (non-blocking with messaging)
 * For bidirectional communication via `window.nx`:
 * ```typescript
 * const applet = new Switch.WebApplet('https://myapp.com');
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
 * await applet.start(); // non-blocking
 * ```
 */
export class WebApplet extends EventTarget {
	#native: any;
	#pollInterval: any = null;
	#started = false;
	#url: string | undefined;
	#documentPath: string | undefined;
	#jsExtension = false;
	#bootHidden = false;

	constructor(url?: string) {
		super();
		this.#url = url;
		this.#native = $.webAppletNew();
	}

	/**
	 * The URL to navigate to (online mode).
	 */
	get url(): string | undefined {
		return this.#url;
	}

	set url(value: string) {
		this.#url = value;
		this.#documentPath = undefined;
	}

	/**
	 * Path to an HTML document in the app's romfs (offline mode).
	 * When set, the applet uses the offline browser instead of loading a URL.
	 * @example '/index.html'
	 */
	get documentPath(): string | undefined {
		return this.#documentPath;
	}

	set documentPath(value: string) {
		this.#documentPath = value;
		this.#url = undefined;
	}

	/**
	 * Enable the `window.nx` JavaScript API in the browser for
	 * bidirectional messaging. Only useful with `start()` (session mode).
	 */
	get jsExtension() {
		return this.#jsExtension;
	}

	set jsExtension(value: boolean) {
		this.#jsExtension = value;
	}

	/**
	 * Start the browser hidden (can be shown later with `appear()`).
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
	 * Listen for 'message' and 'exit' events.
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
		if (this.#documentPath) {
			$.webAppletSetDocumentPath(this.#native, this.#documentPath);
		} else if (this.#url) {
			$.webAppletSetUrl(this.#native, this.#url);
		}
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
