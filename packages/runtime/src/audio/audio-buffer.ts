import { def } from '../utils';
import type { AudioContext } from './audio-context';

export interface AudioBufferOptions {
	length: number;
	numberOfChannels?: number;
	sampleRate: number;
}

/**
 * Represents a short audio asset residing in memory, created from an audio file using the
 * {@link AudioContext.decodeAudioData} method, or from raw data using {@link AudioContext.createBuffer}.
 * Once put into an AudioBuffer, the audio can then be played by being passed into an AudioBufferSourceNode.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer
 */
export class AudioBuffer implements globalThis.AudioBuffer {
	readonly length: number;
	readonly numberOfChannels: number;
	readonly sampleRate: number;
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/AudioBuffer
	 */
	constructor(opts: AudioBufferOptions) {
		this.length = opts.length;
		this.numberOfChannels = opts.numberOfChannels || 1;
		this.sampleRate = opts.sampleRate;
	}
	get duration() {
		return this.length / this.sampleRate;
	}
	copyFromChannel(
		destination: Float32Array,
		channelNumber: number,
		bufferOffset?: number | undefined
	): void {
		throw new Error('Method not implemented.');
	}
	copyToChannel(
		source: Float32Array,
		channelNumber: number,
		bufferOffset?: number | undefined
	): void {
		throw new Error('Method not implemented.');
	}
	getChannelData(channel: number): Float32Array {
		throw new Error('Method not implemented.');
	}
}
def(AudioBuffer);
