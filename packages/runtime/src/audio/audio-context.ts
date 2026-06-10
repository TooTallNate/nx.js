import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import { performance } from '../performance';
import { def } from '../utils';
import { BaseAudioContext, setContextState } from './base-audio-context';
import { ctxInternal as _ } from './internal';

export interface AudioContextOptions {
	/**
	 * Hint for the playback latency tradeoff. Accepted for API compatibility;
	 * nx.js always uses its fixed double-buffered output latency.
	 */
	latencyHint?: AudioContextLatencyCategory | number;
	/**
	 * Sample rate for the audio graph (defaults to `48000`, the native rate
	 * of the Switch audio renderer).
	 */
	sampleRate?: number;
}

/**
 * An audio-processing graph built from audio modules ({@link AudioNode}s)
 * linked together, rendered in real-time to the console's audio output.
 *
 * @example
 *
 * ```typescript
 * const ctx = new AudioContext();
 * const res = await fetch('romfs:/jump.ogg');
 * const buffer = await ctx.decodeAudioData(await res.arrayBuffer());
 *
 * const source = ctx.createBufferSource();
 * source.buffer = buffer;
 * source.connect(ctx.destination);
 * source.start();
 * ```
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioContext
 */
export class AudioContext
	extends BaseAudioContext
	implements globalThis.AudioContext
{
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/AudioContext
	 */
	constructor(options: AudioContextOptions = {}) {
		const sampleRate = options.sampleRate ?? 48000;
		const handle = $.audioContextNew(sampleRate, false);
		// There is no autoplay policy on the console, so the context begins
		// in the "running" state.
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			handle,
			state: 'running',
			sampleRate,
		});
		// Deterministically release the audio output (and join the audio
		// render thread) when the application exits.
		addEventListener('unload', () => {
			const i = _(this);
			if (i.state !== 'closed') {
				$.audioContextClose(i.handle);
				i.state = 'closed';
			}
		});
	}

	/**
	 * The number of seconds of processing latency incurred by the audio graph
	 * itself (one render quantum).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/baseLatency
	 */
	get baseLatency(): number {
		return 128 / this.sampleRate;
	}

	/**
	 * Estimated output latency: the double-buffered (2 × 1024 frame) audio
	 * output stream.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/outputLatency
	 */
	get outputLatency(): number {
		return 2048 / this.sampleRate;
	}

	/**
	 * Returns a new `AudioTimestamp` containing two correlated context/
	 * performance timestamp values.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/getOutputTimestamp
	 */
	getOutputTimestamp(): AudioTimestamp {
		return {
			contextTime: this.currentTime,
			performanceTime: performance.now(),
		};
	}

	/**
	 * Resumes the progression of time in an audio context that has previously
	 * been suspended.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/resume
	 */
	async resume(): Promise<void> {
		const i = _(this);
		if (i.state === 'closed') {
			throw new DOMException(
				"Failed to execute 'resume' on 'AudioContext': Cannot resume a closed AudioContext.",
				'InvalidStateError',
			);
		}
		$.audioContextResume(i.handle);
		setContextState(this, 'running');
	}

	/**
	 * Suspends the progression of time in the audio context, temporarily
	 * halting audio rendering (and reducing CPU/battery usage).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/suspend
	 */
	async suspend(): Promise<void> {
		const i = _(this);
		if (i.state === 'closed') {
			throw new DOMException(
				"Failed to execute 'suspend' on 'AudioContext': Cannot suspend a closed AudioContext.",
				'InvalidStateError',
			);
		}
		$.audioContextSuspend(i.handle);
		setContextState(this, 'suspended');
	}

	/**
	 * Closes the audio context, releasing the audio output resources that it
	 * uses.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/close
	 */
	async close(): Promise<void> {
		const i = _(this);
		if (i.state === 'closed') {
			throw new DOMException(
				"Failed to execute 'close' on 'AudioContext': Cannot close a closed AudioContext.",
				'InvalidStateError',
			);
		}
		$.audioContextClose(i.handle);
		setContextState(this, 'closed');
	}

	createMediaElementSource(
		mediaElement: HTMLMediaElement,
	): MediaElementAudioSourceNode {
		throw new Error('Method not implemented.');
	}
	createMediaStreamDestination(): MediaStreamAudioDestinationNode {
		throw new Error('Method not implemented.');
	}
	createMediaStreamSource(
		mediaStream: MediaStream,
	): MediaStreamAudioSourceNode {
		throw new Error('Method not implemented.');
	}
}
def(AudioContext);
