import { createInternal, def } from '../utils';
import type { AudioContext } from './audio-context';

export interface AudioBufferOptions {
	length: number;
	numberOfChannels?: number;
	sampleRate: number;
}

interface AudioBufferInternal {
	length: number;
	numberOfChannels: number;
	sampleRate: number;
	data: Float32Array[];
}

const _ = createInternal<AudioBuffer, AudioBufferInternal>();

/**
 * Represents a short audio asset residing in memory, created from an audio file using the
 * {@link AudioContext.decodeAudioData} method, or from raw data using {@link AudioContext.createBuffer}.
 * Once put into an AudioBuffer, the audio can then be played by being passed into an AudioBufferSourceNode.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer
 */
export class AudioBuffer implements globalThis.AudioBuffer {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/AudioBuffer
	 */
	constructor(opts: AudioBufferOptions) {
		const num = opts.numberOfChannels || 1;
		_.set(this, {
			length: opts.length,
			numberOfChannels: num,
			sampleRate: opts.sampleRate,
			data: new Array(num),
		});
	}

	get length(): number {
		return _(this).length;
	}

	get numberOfChannels(): number {
		return _(this).numberOfChannels;
	}

	get sampleRate(): number {
		return _(this).sampleRate;
	}

	get duration() {
		return this.length / this.sampleRate;
	}

	copyFromChannel(
		destination: Float32Array,
		channelNumber: number,
		bufferOffset?: number
	): void {
		throw new Error('Method not implemented.');
	}

	copyToChannel(
		source: Float32Array,
		channelNumber: number,
		bufferOffset?: number
	): void {
		throw new Error('Method not implemented.');
	}

	/**
	 * Returns a `Float32Array` containing the PCM data associated with the requested channel.
	 *
	 * @param channel An index representing the particular channel to get data for. An index value of 0 represents the first channel.
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/getChannelData
	 */
	getChannelData(channel: number): Float32Array {
		const i = _(this);
		if (channel < 0 || channel >= i.numberOfChannels) {
			throw new Error(`Channel is invalid: ${channel}`);
		}
		let arr = i.data[channel];
		if (!arr) {
			arr = i.data[channel] = new Float32Array(i.length);
		}
		return arr;
	}
}
def(AudioBuffer);
