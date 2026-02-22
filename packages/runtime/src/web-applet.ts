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
 * Requires Application mode (NSP install or hbmenu via title override).
 *
 * @example Non-blocking with messaging
 * ```typescript
 * const applet = new Switch.WebApplet('https://myapp.example.com');
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
	#bootDisplayKind?: 'default' | 'white' | 'black';
	#backgroundKind?: 'default';
	#footer?: boolean;
	#pointer?: boolean;
	#leftStickMode?: 'pointer' | 'cursor';
	#bootAsMediaPlayer?: boolean;
	#screenShot?: boolean;
	#pageCache?: boolean;
	#webAudio?: boolean;
	#footerFixedKind?: 'default' | 'always' | 'hidden';
	#pageFade?: boolean;
	#bootLoadingIcon?: boolean;
	#pageScrollIndicator?: boolean;
	#mediaPlayerSpeedControl?: boolean;
	#mediaAutoPlay?: boolean;
	#overrideWebAudioVolume?: number;
	#overrideMediaAudioVolume?: number;
	#mediaPlayerAutoClose?: boolean;
	#mediaPlayerUi?: boolean;
	#userAgentAdditionalString?: string;

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

	/** Boot display screen color. */
	get bootDisplayKind() { return this.#bootDisplayKind; }
	set bootDisplayKind(value: 'default' | 'white' | 'black' | undefined) { this.#bootDisplayKind = value; }

	/** Background kind. */
	get backgroundKind() { return this.#backgroundKind; }
	set backgroundKind(value: 'default' | undefined) { this.#backgroundKind = value; }

	/** Whether to show the footer (address bar). */
	get footer() { return this.#footer; }
	set footer(value: boolean | undefined) { this.#footer = value; }

	/** Whether to show a pointer cursor. */
	get pointer() { return this.#pointer; }
	set pointer(value: boolean | undefined) { this.#pointer = value; }

	/** Left stick behavior: 'pointer' (move pointer) or 'cursor' (scroll). */
	get leftStickMode() { return this.#leftStickMode; }
	set leftStickMode(value: 'pointer' | 'cursor' | undefined) { this.#leftStickMode = value; }

	/** Boot as a media player applet. */
	get bootAsMediaPlayer() { return this.#bootAsMediaPlayer; }
	set bootAsMediaPlayer(value: boolean | undefined) { this.#bootAsMediaPlayer = value; }

	/** Enable/disable screenshot capture (Web mode only). */
	get screenShot() { return this.#screenShot; }
	set screenShot(value: boolean | undefined) { this.#screenShot = value; }

	/** Enable/disable the page cache. */
	get pageCache() { return this.#pageCache; }
	set pageCache(value: boolean | undefined) { this.#pageCache = value; }

	/** Enable/disable Web Audio API. */
	get webAudio() { return this.#webAudio; }
	set webAudio(value: boolean | undefined) { this.#webAudio = value; }

	/** Footer fixed display kind. */
	get footerFixedKind() { return this.#footerFixedKind; }
	set footerFixedKind(value: 'default' | 'always' | 'hidden' | undefined) { this.#footerFixedKind = value; }

	/** Enable/disable page fade transition. */
	get pageFade() { return this.#pageFade; }
	set pageFade(value: boolean | undefined) { this.#pageFade = value; }

	/** Show/hide the boot loading icon (Offline mode only). */
	get bootLoadingIcon() { return this.#bootLoadingIcon; }
	set bootLoadingIcon(value: boolean | undefined) { this.#bootLoadingIcon = value; }

	/** Show/hide the page scroll indicator. */
	get pageScrollIndicator() { return this.#pageScrollIndicator; }
	set pageScrollIndicator(value: boolean | undefined) { this.#pageScrollIndicator = value; }

	/** Enable/disable media player speed controls. */
	get mediaPlayerSpeedControl() { return this.#mediaPlayerSpeedControl; }
	set mediaPlayerSpeedControl(value: boolean | undefined) { this.#mediaPlayerSpeedControl = value; }

	/** Enable/disable media autoplay. */
	get mediaAutoPlay() { return this.#mediaAutoPlay; }
	set mediaAutoPlay(value: boolean | undefined) { this.#mediaAutoPlay = value; }

	/** Override web audio volume (0.0 to 1.0). */
	get overrideWebAudioVolume() { return this.#overrideWebAudioVolume; }
	set overrideWebAudioVolume(value: number | undefined) { this.#overrideWebAudioVolume = value; }

	/** Override media audio volume (0.0 to 1.0). */
	get overrideMediaAudioVolume() { return this.#overrideMediaAudioVolume; }
	set overrideMediaAudioVolume(value: number | undefined) { this.#overrideMediaAudioVolume = value; }

	/** Enable/disable media player auto-close on end. */
	get mediaPlayerAutoClose() { return this.#mediaPlayerAutoClose; }
	set mediaPlayerAutoClose(value: boolean | undefined) { this.#mediaPlayerAutoClose = value; }

	/** Show/hide media player UI (Offline mode only). */
	get mediaPlayerUi() { return this.#mediaPlayerUi; }
	set mediaPlayerUi(value: boolean | undefined) { this.#mediaPlayerUi = value; }

	/** Additional string appended to the user agent (Web mode only). */
	get userAgentAdditionalString() { return this.#userAgentAdditionalString; }
	set userAgentAdditionalString(value: string | undefined) { this.#userAgentAdditionalString = value; }

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
		const bootDisplayKindMap = { default: 0, white: 1, black: 2 } as const;
		const leftStickModeMap = { pointer: 0, cursor: 1 } as const;
		const footerFixedKindMap = { default: 0, always: 1, hidden: 2 } as const;

		$.webAppletSetUrl(this.#native, this.#url);
		$.webAppletSetJsExtension(this.#native, this.#jsExtension);
		if (this.#bootHidden) {
			$.webAppletSetBootMode(this.#native, 1);
		}
		if (this.#bootDisplayKind !== undefined)
			$.webAppletSetBootDisplayKind(this.#native, bootDisplayKindMap[this.#bootDisplayKind]);
		if (this.#backgroundKind !== undefined)
			$.webAppletSetBackgroundKind(this.#native, 0);
		if (this.#footer !== undefined)
			$.webAppletSetFooter(this.#native, this.#footer);
		if (this.#pointer !== undefined)
			$.webAppletSetPointer(this.#native, this.#pointer);
		if (this.#leftStickMode !== undefined)
			$.webAppletSetLeftStickMode(this.#native, leftStickModeMap[this.#leftStickMode]);
		if (this.#bootAsMediaPlayer !== undefined)
			$.webAppletSetBootAsMediaPlayer(this.#native, this.#bootAsMediaPlayer);
		if (this.#screenShot !== undefined)
			$.webAppletSetScreenShot(this.#native, this.#screenShot);
		if (this.#pageCache !== undefined)
			$.webAppletSetPageCache(this.#native, this.#pageCache);
		if (this.#webAudio !== undefined)
			$.webAppletSetWebAudio(this.#native, this.#webAudio);
		if (this.#footerFixedKind !== undefined)
			$.webAppletSetFooterFixedKind(this.#native, footerFixedKindMap[this.#footerFixedKind]);
		if (this.#pageFade !== undefined)
			$.webAppletSetPageFade(this.#native, this.#pageFade);
		if (this.#bootLoadingIcon !== undefined)
			$.webAppletSetBootLoadingIcon(this.#native, this.#bootLoadingIcon);
		if (this.#pageScrollIndicator !== undefined)
			$.webAppletSetPageScrollIndicator(this.#native, this.#pageScrollIndicator);
		if (this.#mediaPlayerSpeedControl !== undefined)
			$.webAppletSetMediaPlayerSpeedControl(this.#native, this.#mediaPlayerSpeedControl);
		if (this.#mediaAutoPlay !== undefined)
			$.webAppletSetMediaAutoPlay(this.#native, this.#mediaAutoPlay);
		if (this.#overrideWebAudioVolume !== undefined)
			$.webAppletSetOverrideWebAudioVolume(this.#native, this.#overrideWebAudioVolume);
		if (this.#overrideMediaAudioVolume !== undefined)
			$.webAppletSetOverrideMediaAudioVolume(this.#native, this.#overrideMediaAudioVolume);
		if (this.#mediaPlayerAutoClose !== undefined)
			$.webAppletSetMediaPlayerAutoClose(this.#native, this.#mediaPlayerAutoClose);
		if (this.#mediaPlayerUi !== undefined)
			$.webAppletSetMediaPlayerUi(this.#native, this.#mediaPlayerUi);
		if (this.#userAgentAdditionalString !== undefined)
			$.webAppletSetUserAgentAdditionalString(this.#native, this.#userAgentAdditionalString);
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
