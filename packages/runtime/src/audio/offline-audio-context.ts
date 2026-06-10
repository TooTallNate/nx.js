import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { AudioBuffer, createAudioBuffer } from './audio-buffer';
import { BaseAudioContext, setContextState } from './base-audio-context';
import { ctxInternal } from './internal';
import { OfflineAudioCompletionEvent } from './offline-audio-completion-event';
import type { Event } from '../polyfills/event';

export interface OfflineAudioContextOptions {
	/** The number of channels for the offline rendering (1 or 2 in nx.js). */
	numberOfChannels?: number;
	/** The length of the rendering, in sample-frames. */
	length: number;
	/** The sample rate of the rendering, in Hz. */
	sampleRate: number;
}

interface OfflineAudioContextInternal {
	length: number;
	numberOfChannels: number;
	started: boolean;
}

const _ = createInternal<OfflineAudioContext, OfflineAudioContextInternal>();

/**
 * An {@link AudioContext} which doesn't render the audio graph to the audio
 * output device, but instead generates it (as fast as possible) into an
 * {@link AudioBuffer}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/OfflineAudioContext
 */
export class OfflineAudioContext
	extends BaseAudioContext
	implements globalThis.OfflineAudioContext
{
	oncomplete:
		| ((this: OfflineAudioContext, ev: OfflineAudioCompletionEvent) => any)
		| null;

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/OfflineAudioContext/OfflineAudioContext
	 */
	constructor(contextOptions: OfflineAudioContextOptions);
	constructor(numberOfChannels: number, length: number, sampleRate: number);
	constructor(
		arg0: OfflineAudioContextOptions | number,
		lengthArg?: number,
		sampleRateArg?: number,
	) {
		let numberOfChannels: number;
		let length: number;
		let sampleRate: number;
		if (typeof arg0 === 'object' && arg0 !== null) {
			numberOfChannels = arg0.numberOfChannels ?? 1;
			length = arg0.length;
			sampleRate = arg0.sampleRate;
		} else {
			numberOfChannels = arg0;
			length = lengthArg!;
			sampleRate = sampleRateArg!;
		}
		if (
			!Number.isInteger(numberOfChannels) ||
			numberOfChannels < 1 ||
			numberOfChannels > 2
		) {
			throw new DOMException(
				`The number of channels provided (${numberOfChannels}) is outside the range [1, 2]`,
				'NotSupportedError',
			);
		}
		if (!Number.isInteger(length) || length < 1) {
			throw new DOMException(
				`The number of frames provided (${length}) is less than the minimum bound (1)`,
				'NotSupportedError',
			);
		}
		if (!(sampleRate >= 8000 && sampleRate <= 192000)) {
			throw new DOMException(
				`The sample rate provided (${sampleRate}) is outside the range [8000, 192000]`,
				'NotSupportedError',
			);
		}
		const handle = $.audioContextNew(sampleRate, true);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			handle,
			state: 'suspended',
			sampleRate,
		});
		this.oncomplete = null;
		_.set(this, { length, numberOfChannels, started: false });
	}

	/**
	 * The length of the rendering, in sample-frames.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OfflineAudioContext/length
	 */
	get length(): number {
		return _(this).length;
	}

	/**
	 * Starts rendering the audio graph, resolving with the rendered
	 * {@link AudioBuffer} once complete.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OfflineAudioContext/startRendering
	 */
	async startRendering(): Promise<AudioBuffer> {
		const i = _(this);
		if (i.started) {
			throw new DOMException(
				"Failed to execute 'startRendering' on 'OfflineAudioContext': cannot call startRendering more than once",
				'InvalidStateError',
			);
		}
		i.started = true;
		setContextState(this, 'running');
		const channelData = await $.audioOfflineRender(
			ctxInternal(this).handle,
			i.numberOfChannels,
			i.length,
		);
		const buffer = createAudioBuffer(
			channelData.map((ab) => new Float32Array(ab)),
			this.sampleRate,
		);
		setContextState(this, 'closed');
		this.dispatchEvent(
			new OfflineAudioCompletionEvent('complete', {
				renderedBuffer: buffer,
			}),
		);
		return buffer;
	}

	/**
	 * Not currently supported in nx.js.
	 */
	async suspend(suspendTime: number): Promise<void> {
		throw new DOMException(
			"'suspend' is not supported on 'OfflineAudioContext' in nx.js",
			'NotSupportedError',
		);
	}

	/**
	 * Not currently supported in nx.js.
	 */
	async resume(): Promise<void> {
		throw new DOMException(
			"'resume' is not supported on 'OfflineAudioContext' in nx.js",
			'NotSupportedError',
		);
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'complete') {
			this.oncomplete?.(event as OfflineAudioCompletionEvent);
		}
		return super.dispatchEvent(event);
	}
}
def(OfflineAudioContext);
