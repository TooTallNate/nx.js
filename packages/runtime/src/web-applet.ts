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
 * Configuration options for {@link WebApplet.start | `WebApplet.start()`}.
 * All values are optional and only take effect at launch time.
 */
export interface WebAppletOptions {
	/** Enable the `window.nx` JavaScript API in the browser for bidirectional messaging. */
	jsExtension?: boolean;
	/** Start the browser hidden (can be shown later with {@link WebApplet.appear | `appear()`}). */
	bootHidden?: boolean;
	/** Whether to show the footer (address bar). */
	footer?: boolean;
	/** Whether to show a pointer cursor. */
	pointer?: boolean;
	/** Left stick behavior: `'pointer'` (move pointer) or `'cursor'` (scroll). */
	leftStickMode?: 'pointer' | 'cursor';
	/** Boot display screen color. */
	bootDisplayKind?: 'default' | 'white' | 'black';
	/** Background kind. */
	backgroundKind?: 'default';
	/** Footer fixed display kind. */
	footerFixedKind?: 'default' | 'always' | 'hidden';
	/** Boot as a media player applet. */
	bootAsMediaPlayer?: boolean;
	/** Enable/disable screenshot capture (Web mode only). */
	screenShot?: boolean;
	/** Enable/disable the page cache. */
	pageCache?: boolean;
	/** Enable/disable Web Audio API. */
	webAudio?: boolean;
	/** Enable/disable page fade transition. */
	pageFade?: boolean;
	/** Show/hide the boot loading icon (Offline mode only). */
	bootLoadingIcon?: boolean;
	/** Show/hide the page scroll indicator. */
	pageScrollIndicator?: boolean;
	/** Enable/disable media player speed controls. */
	mediaPlayerSpeedControl?: boolean;
	/** Enable/disable media autoplay. */
	mediaAutoPlay?: boolean;
	/** Override web audio volume (0.0 to 1.0). */
	overrideWebAudioVolume?: number;
	/** Override media audio volume (0.0 to 1.0). */
	overrideMediaAudioVolume?: number;
	/** Enable/disable media player auto-close on end. */
	mediaPlayerAutoClose?: boolean;
	/** Show/hide media player UI (Offline mode only). */
	mediaPlayerUi?: boolean;
	/** Additional string appended to the user agent (Web mode only). */
	userAgentAdditionalString?: string;
}

/**
 * Opens the Switch's built-in web browser as a Library Applet.
 * Requires Application mode (NSP install or hbmenu via title override).
 *
 * @example Non-blocking with messaging
 * ```typescript
 * const applet = new Switch.WebApplet('https://myapp.example.com');
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
 * await applet.start({ jsExtension: true });
 * ```
 */
export class WebApplet extends EventTarget {
	#native: any;
	#pollInterval: any = null;
	#started = false;
	#url: string;

	constructor(url: string) {
		super();
		this.#url = url;
		this.#native = $.webAppletNew();
	}

	/**
	 * Whether the browser applet is currently running.
	 */
	get running() {
		if (!this.#started) return false;
		return $.webAppletIsRunning(this.#native) as boolean;
	}

	/**
	 * The current operating mode:
	 * - `'web-session'` — HTTP/HTTPS URL with WebSession (supports `window.nx` messaging)
	 * - `'offline'` — Offline applet loading HTML from app's html-document NCA (supports `window.nx` messaging)
	 * - `'none'` — not started
	 */
	get mode(): string {
		return $.webAppletGetMode(this.#native) as string;
	}

	/**
	 * Opens the browser. The nx.js event loop continues running.
	 * Pass an options object to configure the applet before launch.
	 * Listen for `'message'` and `'exit'` events.
	 */
	async start(options?: WebAppletOptions): Promise<void> {
		if (this.#started) {
			throw new Error('WebApplet already started');
		}

		$.webAppletStart(this.#native, this.#url, options ?? {});
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
	 * Requires `jsExtension: true` in the start options.
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

	#cleanup() {
		this.#started = false;
		if (this.#pollInterval !== null) {
			clearInterval(this.#pollInterval);
			this.#pollInterval = null;
		}
	}
}

def(WebApplet);
