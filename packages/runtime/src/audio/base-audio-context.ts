import { INTERNAL_SYMBOL } from '../internal';
import { Event } from '../polyfills/event';
import { EventTarget } from '../polyfills/event-target';
import { assertInternalConstructor, createInternal, def } from '../utils';
import { AudioBuffer } from './audio-buffer';
import { AudioDestinationNode } from './audio-destination-node';

interface BaseAudioContextInternal {
	destination?: AudioDestinationNode;
}

const _ = createInternal<BaseAudioContext, BaseAudioContextInternal>();

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
	}
	get audioWorklet(): AudioWorklet {
		throw new Error('Method not implemented.');
	}
	get currentTime(): number {
		throw new Error('Method not implemented.');
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
	get sampleRate(): number {
		throw new Error('Method not implemented.');
	}
	get state(): AudioContextState {
		throw new Error('Method not implemented.');
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
		throw new Error('Method not implemented.');
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
		constraints?: PeriodicWaveConstraints | undefined
	): PeriodicWave;
	createPeriodicWave(
		real: Iterable<number>,
		imag: Iterable<number>,
		constraints?: PeriodicWaveConstraints | undefined
	): PeriodicWave;
	createPeriodicWave(
		real: unknown,
		imag: unknown,
		constraints?: unknown
	): PeriodicWave {
		throw new Error('Method not implemented.');
	}
	createScriptProcessor(
		bufferSize?: number | undefined,
		numberOfInputChannels?: number | undefined,
		numberOfOutputChannels?: number | undefined
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
		successCallback?: DecodeSuccessCallback | null | undefined,
		errorCallback?: DecodeErrorCallback | null | undefined
	): Promise<AudioBuffer> {
		throw new Error('Method not implemented.');
	}
}
def(BaseAudioContext);
