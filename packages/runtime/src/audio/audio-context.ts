import { def } from '../utils';
import { INTERNAL_SYMBOL } from '../internal';
import { BaseAudioContext, _ } from './base-audio-context';
import { Event } from '../polyfills/event';
import { performance } from '../performance';
import type { AudioTimestamp } from '../types';

/**
 * @see https://developer.mozilla.org/docs/Web/API/AudioContext
 */
export class AudioContext
	extends BaseAudioContext
	implements globalThis.AudioContext
{
	baseLatency: number;
	outputLatency: number;
	constructor() {
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL);
		this.baseLatency = 0;
		this.outputLatency = 0;
	}

	/**
	 * Closes the audio context, releasing any system audio resources that it uses.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/close
	 */
	async close(): Promise<void> {
		const i = _(this);
		if (i.state === 'closed') {
			throw new Error(`Can't close an AudioContext twice`);
		}
		i.state = 'closed';
		this.dispatchEvent(new Event('statechange'));
	}

	/**
	 * Resumes the progression of time in an audio context that has previously been suspended.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/resume
	 */
	async resume(): Promise<void> {
		const i = _(this);
		if (i.state === 'closed') {
			throw new Error(`Can't resume if the control thread state is "closed"`);
		}
		if (i.state === 'running') return;
		i.state = 'running';
		this.dispatchEvent(new Event('statechange'));
	}

	/**
	 * Suspends the progression of time in the audio context, temporarily halting audio
	 * hardware access and reducing CPU/battery usage in the process — this is useful
	 * if you want an application to power down the audio hardware when it will not be
	 * using an audio context for a while.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioContext/suspend
	 */
	async suspend(): Promise<void> {
		const i = _(this);
		if (i.state === 'closed') {
			throw new Error(`Can't suspend if the control thread state is "closed"`);
		}
		if (i.state === 'suspended') return;
		i.state = 'suspended';
		this.dispatchEvent(new Event('statechange'));
	}

	/**
	 * Returns a new `AudioTimestamp` object containing two audio timestamp values
	 * relating to the current audio context.
	 *
	 * @see https://developer.mozilla.org/en-US/docs/Web/API/AudioContext/getOutputTimestamp
	 */
	getOutputTimestamp(): AudioTimestamp {
		return {
			contextTime: this.currentTime,
			performanceTime: performance.now()
		}
	}

	createMediaElementSource(
		mediaElement: HTMLMediaElement
	): MediaElementAudioSourceNode {
		throw new Error('Method not implemented.');
	}
	createMediaStreamDestination(): MediaStreamAudioDestinationNode {
		throw new Error('Method not implemented.');
	}
	createMediaStreamSource(
		mediaStream: MediaStream
	): MediaStreamAudioSourceNode {
		throw new Error('Method not implemented.');
	}
}
def(AudioContext);
