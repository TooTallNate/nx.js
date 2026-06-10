import { DOMException } from '../dom-exception';
import { def } from '../utils';
import { bufferInternal as _ } from './internal';

export interface AudioBufferOptions {
	/** The size of the buffer in sample-frames. */
	length: number;
	/** The number of channels for the buffer (defaults to `1`). */
	numberOfChannels?: number;
	/** The sample rate in Hz for the buffer. */
	sampleRate: number;
}

/**
 * Adopt already-allocated channel data (e.g. from `decodeAudioData()`)
 * without copying.
 *
 * @internal
 */
export function createAudioBuffer(
	channels: Float32Array[],
	sampleRate: number,
): AudioBuffer {
	const buffer = Object.create(AudioBuffer.prototype) as AudioBuffer;
	_.set(buffer, {
		channels,
		sampleRate,
		length: channels[0]?.length ?? 0,
	});
	return buffer;
}

/**
 * A short audio asset residing in memory, created from an audio file using
 * {@link BaseAudioContext.decodeAudioData | `decodeAudioData()`}, or from raw
 * data using {@link BaseAudioContext.createBuffer | `createBuffer()`}. Once
 * put into an `AudioBuffer`, the audio can be played by being passed into an
 * {@link AudioBufferSourceNode}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer
 */
export class AudioBuffer implements globalThis.AudioBuffer {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/AudioBuffer
	 */
	constructor(options: AudioBufferOptions) {
		if (typeof options !== 'object' || options === null) {
			throw new TypeError(
				"Failed to construct 'AudioBuffer': 1 argument required",
			);
		}
		const numberOfChannels = options.numberOfChannels ?? 1;
		const { length, sampleRate } = options;
		if (
			!Number.isInteger(numberOfChannels) ||
			numberOfChannels < 1 ||
			numberOfChannels > 32
		) {
			throw new DOMException(
				`The number of channels provided (${numberOfChannels}) is outside the range [1, 32]`,
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
		const channels: Float32Array[] = [];
		for (let i = 0; i < numberOfChannels; i++) {
			channels.push(new Float32Array(length));
		}
		_.set(this, { channels, sampleRate, length });
	}

	/**
	 * Duration of the PCM audio data stored in the buffer, in seconds.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/duration
	 */
	get duration(): number {
		const i = _(this);
		return i.length / i.sampleRate;
	}

	/**
	 * Length of the PCM audio data stored in the buffer, in sample-frames.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/length
	 */
	get length(): number {
		return _(this).length;
	}

	/**
	 * The number of discrete audio channels stored in the buffer.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/numberOfChannels
	 */
	get numberOfChannels(): number {
		return _(this).channels.length;
	}

	/**
	 * The sample rate of the PCM audio data, in samples per second.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/sampleRate
	 */
	get sampleRate(): number {
		return _(this).sampleRate;
	}

	/**
	 * Returns the `Float32Array` containing the PCM data for the requested
	 * channel.
	 *
	 * > [!NOTE]
	 * > In nx.js, the returned array is a live view of the data the audio
	 * > render thread reads — mutations are heard, even during playback.
	 *
	 * @param channel An index representing the channel to get data for (`0` is the first channel).
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/getChannelData
	 */
	getChannelData(channel: number): Float32Array {
		const i = _(this);
		if (channel < 0 || channel >= i.channels.length) {
			throw new DOMException(
				`The channel index provided (${channel}) is outside the range [0, ${
					i.channels.length - 1
				}]`,
				'IndexSizeError',
			);
		}
		return i.channels[channel];
	}

	/**
	 * Copies the samples from the specified channel into the `destination` array.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/copyFromChannel
	 */
	copyFromChannel(
		destination: Float32Array,
		channelNumber: number,
		bufferOffset = 0,
	): void {
		const data = this.getChannelData(channelNumber);
		if (bufferOffset >= data.length) return;
		const count = Math.min(data.length - bufferOffset, destination.length);
		destination.set(data.subarray(bufferOffset, bufferOffset + count));
	}

	/**
	 * Copies the samples from the `source` array into the specified channel.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBuffer/copyToChannel
	 */
	copyToChannel(
		source: Float32Array,
		channelNumber: number,
		bufferOffset = 0,
	): void {
		const data = this.getChannelData(channelNumber);
		if (bufferOffset >= data.length) return;
		const count = Math.min(data.length - bufferOffset, source.length);
		data.set(source.subarray(0, count), bufferOffset);
	}
}
def(AudioBuffer);
