import { INTERNAL_SYMBOL } from '../internal';
import { Event } from '../polyfills/event';
import { EventTarget } from '../polyfills/event-target';
import { assertInternalConstructor, createInternal, def } from '../utils';
import { AudioBuffer } from './audio-buffer';
import { AudioDestinationNode } from './audio-destination-node';
import { AudioBufferSourceNode } from './audio-buffer-source-node';
import type { AudioContext } from './audio-context';
import type { AudioContextState, DOMHighResTimeStamp } from '../types';

export interface BaseAudioContextInternal {
	sampleRate: number;
	state: AudioContextState;
	destination?: AudioDestinationNode;
	currentTime: DOMHighResTimeStamp;
}

export const _ = createInternal<BaseAudioContext, BaseAudioContextInternal>();

/**
 * Base definition for online and offline audio-processing graphs, as represented
 * by {@link AudioContext | `AudioContext`} and `OfflineAudioContext` respectively.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext
 */
export class BaseAudioContext
	extends EventTarget
	implements globalThis.BaseAudioContext
{
	onstatechange: ((this: BaseAudioContext, ev: Event) => any) | null;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		this.onstatechange = null;
		_.set(this, {
			sampleRate: 48000,
			state: 'suspended',
			currentTime: 0,
		});
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'statechange') {
			this.onstatechange?.(event);
		}
		return super.dispatchEvent(event);
	}

	get audioWorklet(): AudioWorklet {
		throw new Error('Method not implemented.');
	}
	get currentTime(): number {
		return _(this).currentTime;
	}
	get destination(): AudioDestinationNode {
		const i = _(this);
		let d = i.destination;
		if (!d) {
			// @ts-expect-error internal constructor
			d = i.destination = new AudioDestinationNode(INTERNAL_SYMBOL, this);
		}
		return d;
	}
	get listener(): AudioListener {
		throw new Error('Method not implemented.');
	}

	/**
	 * A floating point number indicating the audio context's sample rate, in samples per second.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/sampleRate
	 */
	get sampleRate(): number {
		return _(this).sampleRate;
	}

	/**
	 * Read-only property representing the current state of the audio context.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/state
	 */
	get state(): AudioContextState {
		return _(this).state;
	}

	createAnalyser(): AnalyserNode {
		throw new Error('Method not implemented.');
	}
	createBiquadFilter(): BiquadFilterNode {
		throw new Error('Method not implemented.');
	}
	createBuffer(
		numberOfChannels: number,
		length: number,
		sampleRate: number
	): AudioBuffer {
		return new AudioBuffer({ numberOfChannels, length, sampleRate });
	}
	createBufferSource(): AudioBufferSourceNode {
		return new AudioBufferSourceNode(this);
	}
	createChannelMerger(
		numberOfInputs?: number | undefined
	): ChannelMergerNode {
		throw new Error('Method not implemented.');
	}
	createChannelSplitter(
		numberOfOutputs?: number | undefined
	): ChannelSplitterNode {
		throw new Error('Method not implemented.');
	}
	createConstantSource(): ConstantSourceNode {
		throw new Error('Method not implemented.');
	}
	createConvolver(): ConvolverNode {
		throw new Error('Method not implemented.');
	}
	createDelay(maxDelayTime?: number | undefined): DelayNode {
		throw new Error('Method not implemented.');
	}
	createDynamicsCompressor(): DynamicsCompressorNode {
		throw new Error('Method not implemented.');
	}
	createGain(): GainNode {
		throw new Error('Method not implemented.');
	}
	createIIRFilter(feedforward: number[], feedback: number[]): IIRFilterNode;
	createIIRFilter(
		feedforward: Iterable<number>,
		feedback: Iterable<number>
	): IIRFilterNode;
	createIIRFilter(feedforward: unknown, feedback: unknown): IIRFilterNode {
		throw new Error('Method not implemented.');
	}
	createOscillator(): OscillatorNode {
		throw new Error('Method not implemented.');
	}
	createPanner(): PannerNode {
		throw new Error('Method not implemented.');
	}
	createPeriodicWave(
		real: number[] | Float32Array,
		imag: number[] | Float32Array,
		constraints?: PeriodicWaveConstraints
	): PeriodicWave;
	createPeriodicWave(
		real: Iterable<number>,
		imag: Iterable<number>,
		constraints?: PeriodicWaveConstraints
	): PeriodicWave;
	createPeriodicWave(
		real: unknown,
		imag: unknown,
		constraints?: unknown
	): PeriodicWave {
		throw new Error('Method not implemented.');
	}
	createScriptProcessor(
		bufferSize?: number,
		numberOfInputChannels?: number,
		numberOfOutputChannels?: number
	): ScriptProcessorNode {
		throw new Error('Method not implemented.');
	}
	createStereoPanner(): StereoPannerNode {
		throw new Error('Method not implemented.');
	}
	createWaveShaper(): WaveShaperNode {
		throw new Error('Method not implemented.');
	}
	decodeAudioData(
		audioData: ArrayBuffer,
		successCallback?: DecodeSuccessCallback | null,
		errorCallback?: DecodeErrorCallback | null
	): Promise<AudioBuffer> {
		throw new Error('Method not implemented.');
	}
}
def(BaseAudioContext);
