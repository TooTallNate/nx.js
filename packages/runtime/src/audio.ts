import { $ } from './$';
import { DOMException } from './dom-exception';
import { fetch } from './fetch/fetch';
import { ErrorEvent, Event } from './polyfills/event';
import { EventTarget } from './polyfills/event-target';
import { URL } from './polyfills/url';
import { clearInterval, setInterval } from './timers';
import { def } from './utils';

const HAVE_NOTHING = 0;
const HAVE_METADATA = 1;
const HAVE_CURRENT_DATA = 2;
const HAVE_FUTURE_DATA = 3;
const HAVE_ENOUGH_DATA = 4;

interface DecodedAudio {
	pcmData: ArrayBuffer;
	sampleRate: number;
	channels: number;
	samples: number;
	byteLength: number;
}

function mimeFromExtension(path: string): string {
	const ext = path.split('.').pop()?.toLowerCase();
	switch (ext) {
		case 'mp3':
			return 'audio/mpeg';
		case 'wav':
			return 'audio/wav';
		case 'ogg':
			return 'audio/ogg';
		default:
			return 'application/octet-stream';
	}
}

let audioInitialized = false;
let updateInterval: ReturnType<typeof setInterval> | null = null;

function ensureAudioInit() {
	if (!audioInitialized) {
		$.audioInit();
		audioInitialized = true;
		addEventListener('unload', () => {
			$.audioExit();
			audioInitialized = false;
		});
	}
}

function ensureUpdateLoop() {
	if (!updateInterval) {
		updateInterval = setInterval(() => {
			$.audioUpdate();
		}, 16); // ~60fps
	}
}

/**
 * The `Audio` class provides audio playback functionality similar to the
 * [`HTMLAudioElement`](https://developer.mozilla.org/docs/Web/API/HTMLAudioElement)
 * in web browsers.
 *
 * ### Supported Audio Formats
 *
 * - `mp3` — MP3 audio using [dr_mp3](https://github.com/mackron/dr_libs)
 * - `wav` — WAV audio using [dr_wav](https://github.com/mackron/dr_libs)
 * - `ogg` — OGG Vorbis audio using [stb_vorbis](https://github.com/nothings/stb)
 *
 * @example
 *
 * ```typescript
 * const audio = new Audio('romfs:/music.mp3');
 * audio.addEventListener('canplaythrough', () => {
 *   audio.play();
 * });
 * ```
 */
export class Audio extends EventTarget {
	declare onplay: ((this: Audio, ev: Event) => any) | null;
	declare onpause: ((this: Audio, ev: Event) => any) | null;
	declare onended: ((this: Audio, ev: Event) => any) | null;
	declare onerror: ((this: Audio, ev: ErrorEvent) => any) | null;
	declare oncanplaythrough: ((this: Audio, ev: Event) => any) | null;
	declare onloadedmetadata: ((this: Audio, ev: Event) => any) | null;
	declare ontimeupdate: ((this: Audio, ev: Event) => any) | null;

	#src = '';
	#currentSrc = '';
	#volume = 1.0;
	#loop = false;
	#paused = true;
	#ended = false;
	#playbackRate = 1.0;
	#readyState = HAVE_NOTHING;
	#voiceId = -1;
	#decoded: DecodedAudio | null = null;
	#sampleOffset = 0;
	#timeupdateInterval: ReturnType<typeof setInterval> | null = null;

	constructor(src?: string) {
		super();
		this.onplay = null;
		this.onpause = null;
		this.onended = null;
		this.onerror = null;
		this.oncanplaythrough = null;
		this.onloadedmetadata = null;
		this.ontimeupdate = null;
		if (src) {
			this.src = src;
		}
	}

	get src(): string {
		return this.#src;
	}

	set src(val: string) {
		this.#src = val;
		this.load();
	}

	get currentSrc(): string {
		return this.#currentSrc;
	}

	get volume(): number {
		return this.#volume;
	}

	set volume(val: number) {
		this.#volume = Math.max(0, Math.min(1, val));
		if (this.#voiceId >= 0) {
			$.audioSetVolume(this.#voiceId, this.#volume);
		}
	}

	get loop(): boolean {
		return this.#loop;
	}

	set loop(val: boolean) {
		this.#loop = val;
	}

	get paused(): boolean {
		return this.#paused;
	}

	get ended(): boolean {
		return this.#ended;
	}

	get currentTime(): number {
		if (!this.#decoded) return 0;
		if (this.#voiceId >= 0 && !this.#paused) {
			const played = $.audioGetPlayedSamples(this.#voiceId) as number;
			return (played + this.#sampleOffset) / this.#decoded.sampleRate;
		}
		return this.#sampleOffset / (this.#decoded?.sampleRate ?? 48000);
	}

	set currentTime(val: number) {
		if (!this.#decoded) return;
		this.#sampleOffset = Math.max(
			0,
			Math.min(
				Math.floor(val * this.#decoded.sampleRate),
				this.#decoded.samples,
			),
		);
		// If currently playing, restart at new offset
		if (!this.#paused && this.#voiceId >= 0) {
			$.audioStop(this.#voiceId);
			$.audioPlay(
				this.#decoded.pcmData,
				this.#voiceId,
				this.#volume,
				this.#loop,
				this.#decoded.sampleRate,
				this.#decoded.channels,
				this.#decoded.samples,
			);
		}
	}

	get duration(): number {
		if (!this.#decoded) return NaN;
		return this.#decoded.samples / this.#decoded.sampleRate;
	}

	get playbackRate(): number {
		return this.#playbackRate;
	}

	set playbackRate(val: number) {
		this.#playbackRate = val;
		if (this.#voiceId >= 0) {
			$.audioSetPitch(this.#voiceId, val);
		}
	}

	get readyState(): number {
		return this.#readyState;
	}

	static get HAVE_NOTHING() {
		return HAVE_NOTHING;
	}
	static get HAVE_METADATA() {
		return HAVE_METADATA;
	}
	static get HAVE_CURRENT_DATA() {
		return HAVE_CURRENT_DATA;
	}
	static get HAVE_FUTURE_DATA() {
		return HAVE_FUTURE_DATA;
	}
	static get HAVE_ENOUGH_DATA() {
		return HAVE_ENOUGH_DATA;
	}

	load(): void {
		if (!this.#src) return;

		// Stop current playback
		this.#stop();
		this.#readyState = HAVE_NOTHING;
		this.#decoded = null;

		const url = new URL(this.#src, $.entrypoint);
		this.#currentSrc = url.href;
		const mime = mimeFromExtension(url.pathname);

		fetch(url)
			.then((res) => {
				if (!res.ok) {
					throw new Error(`Failed to load audio: ${res.status}`);
				}
				return res.arrayBuffer();
			})
			.then((buf) => $.audioDecode(buf, mime) as Promise<DecodedAudio>)
			.then(
				(decoded) => {
					this.#decoded = decoded;
					this.#readyState = HAVE_METADATA;
					this.dispatchEvent(new Event('loadedmetadata'));
					this.#readyState = HAVE_ENOUGH_DATA;
					this.dispatchEvent(new Event('canplaythrough'));
				},
				(error) => {
					this.dispatchEvent(new ErrorEvent('error', { error }));
				},
			);
	}

	async play(): Promise<void> {
		if (!this.#decoded) {
			throw new DOMException('No audio source loaded', 'InvalidStateError');
		}

		ensureAudioInit();
		ensureUpdateLoop();

		if (this.#voiceId < 0) {
			this.#voiceId = $.audioAllocVoice() as number;
		}

		this.#paused = false;
		this.#ended = false;

		$.audioPlay(
			this.#decoded.pcmData,
			this.#voiceId,
			this.#volume,
			this.#loop,
			this.#decoded.sampleRate,
			this.#decoded.channels,
			this.#decoded.samples,
		);

		if (this.#playbackRate !== 1.0) {
			$.audioSetPitch(this.#voiceId, this.#playbackRate);
		}

		this.dispatchEvent(new Event('play'));

		// Set up timeupdate + ended checking
		this.#startTimeupdateLoop();
	}

	pause(): void {
		if (this.#paused) return;
		this.#paused = true;
		if (this.#voiceId >= 0) {
			$.audioPause(this.#voiceId, true);
		}
		this.#stopTimeupdateLoop();
		this.dispatchEvent(new Event('pause'));
	}

	#stop(): void {
		this.#stopTimeupdateLoop();
		if (this.#voiceId >= 0) {
			$.audioStop(this.#voiceId);
			$.audioFreeVoice(this.#voiceId);
			this.#voiceId = -1;
		}
		this.#paused = true;
		this.#sampleOffset = 0;
	}

	#startTimeupdateLoop(): void {
		this.#stopTimeupdateLoop();
		this.#timeupdateInterval = setInterval(() => {
			if (this.#voiceId >= 0 && !this.#paused) {
				this.dispatchEvent(new Event('timeupdate'));

				// Check if playback ended
				if (!this.#loop && !$.audioIsPlaying(this.#voiceId)) {
					this.#ended = true;
					this.#paused = true;
					this.#stopTimeupdateLoop();
					this.dispatchEvent(new Event('ended'));
				}
			}
		}, 250);
	}

	#stopTimeupdateLoop(): void {
		if (this.#timeupdateInterval) {
			clearInterval(this.#timeupdateInterval);
			this.#timeupdateInterval = null;
		}
	}

	dispatchEvent(event: Event): boolean {
		const handler = (this as any)[`on${event.type}`];
		if (typeof handler === 'function') {
			handler.call(this, event);
		}
		return super.dispatchEvent(event);
	}
}
def(Audio);
