import { $ } from './$';
import { createAudioBuffer, type AudioBuffer } from './audio/audio-buffer';
import type { AudioBufferSourceNode } from './audio/audio-buffer-source-node';
import { AudioContext } from './audio/audio-context';
import type { GainNode } from './audio/gain-node';
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

// All media elements (`Audio`, `Video`) share one (lazily-created)
// AudioContext.
let sharedCtx: AudioContext | null = null;

/**
 * The shared media-element AudioContext (created on first use).
 *
 * @internal
 */
export function getSharedAudioContext(): AudioContext {
	if (!sharedCtx) {
		sharedCtx = new AudioContext();
	}
	return sharedCtx;
}
const getContext = getSharedAudioContext;

/**
 * The `Audio` class provides audio playback functionality similar to the
 * [`HTMLAudioElement`](https://developer.mozilla.org/docs/Web/API/HTMLAudioElement)
 * in web browsers. It is implemented on top of the Web Audio API
 * ({@link AudioContext}) — for low-level / game audio use the Web Audio
 * classes directly.
 *
 * ### Supported Audio Formats
 *
 * Any audio container/codec supported by [FFmpeg](https://ffmpeg.org),
 * including MP3, WAV, OGG Vorbis, Opus, FLAC and AAC/M4A.
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
	#buffer: AudioBuffer | null = null;
	#gain: GainNode | null = null;
	#source: AudioBufferSourceNode | null = null;
	#offset = 0; // playback position (seconds) at the last start/pause/rebase
	#startCtxTime = 0; // context time at the last start/rebase
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
		if (this.#gain) {
			this.#gain.gain.value = this.#volume;
		}
	}

	get loop(): boolean {
		return this.#loop;
	}

	set loop(val: boolean) {
		this.#loop = val;
		if (this.#source) {
			this.#source.loop = val;
		}
	}

	get paused(): boolean {
		return this.#paused;
	}

	get ended(): boolean {
		return this.#ended;
	}

	get currentTime(): number {
		if (!this.#buffer) return 0;
		if (this.#paused || !this.#source) return this.#offset;
		const ctx = getContext();
		let pos =
			this.#offset +
			(ctx.currentTime - this.#startCtxTime) * this.#playbackRate;
		const dur = this.#buffer.duration;
		if (this.#loop) {
			pos = dur > 0 ? pos % dur : 0;
		} else if (pos > dur) {
			pos = dur;
		}
		return pos;
	}

	set currentTime(val: number) {
		if (!this.#buffer) return;
		const offset = Math.max(0, Math.min(val, this.#buffer.duration));
		this.#offset = offset;
		if (!this.#paused) {
			// Restart playback at the new position.
			this.#stopSource();
			this.#startSource();
		}
	}

	get duration(): number {
		if (!this.#buffer) return NaN;
		return this.#buffer.duration;
	}

	get playbackRate(): number {
		return this.#playbackRate;
	}

	set playbackRate(val: number) {
		if (!this.#paused && this.#source) {
			// Rebase the position tracking at the old rate before switching.
			const ctx = getContext();
			this.#offset = this.currentTime;
			this.#startCtxTime = ctx.currentTime;
			this.#source.playbackRate.value = val;
		}
		this.#playbackRate = val;
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
		this.#buffer = null;

		const url = new URL(this.#src, $.entrypoint);
		this.#currentSrc = url.href;

		fetch(url)
			.then((res) => {
				if (!res.ok) {
					throw new Error(`Failed to load audio: ${res.status}`);
				}
				return res.arrayBuffer();
			})
			.then((buf) => $.audioDecode(buf))
			.then(
				({ channelData, sampleRate }) => {
					this.#buffer = createAudioBuffer(
						channelData.map((ab) => new Float32Array(ab)),
						sampleRate,
					);
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
		if (!this.#buffer) {
			throw new DOMException('No audio source loaded', 'InvalidStateError');
		}
		this.#stopSource();
		this.#paused = false;
		this.#ended = false;
		this.#startSource();
		this.dispatchEvent(new Event('play'));
		this.#startTimeupdateLoop();
	}

	pause(): void {
		if (this.#paused) return;
		this.#offset = this.currentTime;
		this.#stopSource();
		this.#paused = true;
		this.#stopTimeupdateLoop();
		this.dispatchEvent(new Event('pause'));
	}

	#startSource(): void {
		const ctx = getContext();
		if (!this.#gain) {
			this.#gain = ctx.createGain();
			this.#gain.gain.value = this.#volume;
			this.#gain.connect(ctx.destination);
		}
		const source = ctx.createBufferSource();
		source.buffer = this.#buffer;
		source.loop = this.#loop;
		source.playbackRate.value = this.#playbackRate;
		source.connect(this.#gain);
		source.onended = () => {
			// Ignore stale sources (replaced by seek/pause/replay).
			if (this.#source !== source || this.#paused) return;
			this.#ended = true;
			this.#paused = true;
			this.#offset = this.#buffer?.duration ?? 0;
			this.#source = null;
			this.#stopTimeupdateLoop();
			this.dispatchEvent(new Event('ended'));
		};
		this.#source = source;
		this.#startCtxTime = ctx.currentTime;
		source.start(0, this.#offset);
	}

	#stopSource(): void {
		const source = this.#source;
		if (source) {
			this.#source = null;
			source.stop();
			source.disconnect();
		}
	}

	#stop(): void {
		this.#stopTimeupdateLoop();
		this.#stopSource();
		this.#paused = true;
		this.#offset = 0;
	}

	#startTimeupdateLoop(): void {
		this.#stopTimeupdateLoop();
		this.#timeupdateInterval = setInterval(() => {
			if (!this.#paused) {
				this.dispatchEvent(new Event('timeupdate'));
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
