import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { Event } from '../polyfills/event';
import { EventTarget } from '../polyfills/event-target';
import { assertInternalConstructor, def } from '../utils';
import { AudioBuffer, createAudioBuffer } from './audio-buffer';
import { AudioBufferSourceNode } from './audio-buffer-source-node';
import { AudioDestinationNode } from './audio-destination-node';
import { GainNode } from './gain-node';
import { StereoPannerNode } from './stereo-panner-node';
import { ctxInternal as _, type BaseAudioContextInternal } from './internal';
import { INTERNAL_SYMBOL } from '../internal';

/**
 * Transition the context state and fire `statechange` if it changed.
 *
 * @internal
 */
export function setContextState(
	ctx: BaseAudioContext,
	state: AudioContextState,
) {
	const i = _(ctx);
	if (i.state === state) return;
	i.state = state;
	ctx.dispatchEvent(new Event('statechange'));
}

/**
 * Base interface for online and offline audio-processing graphs, as
 * represented by {@link AudioContext} and {@link OfflineAudioContext}
 * respectively.
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
		_.set(this, arguments[1] as BaseAudioContextInternal);
	}

	/**
	 * The time, in seconds, of the sample frame currently being processed by
	 * the audio graph. Starts at `0` and increases in real-time while the
	 * context is running.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/currentTime
	 */
	get currentTime(): number {
		return $.audioContextCurrentTime(_(this).handle);
	}

	/**
	 * The final destination of the audio graph (the audio output device).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/destination
	 */
	get destination(): AudioDestinationNode {
		const i = _(this);
		let d = i.destination;
		if (!d) {
			const handle = $.audioContextDestination(i.handle);
			// @ts-expect-error internal constructor
			d = i.destination = new AudioDestinationNode(INTERNAL_SYMBOL, {
				context: this,
				handle,
				numberOfInputs: 1,
				numberOfOutputs: 0,
				channelCount: 2,
				channelCountMode: 'explicit',
				channelInterpretation: 'speakers',
			});
		}
		return d;
	}

	/**
	 * The sample rate (in samples per second) used by all nodes in this
	 * context.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/sampleRate
	 */
	get sampleRate(): number {
		return _(this).sampleRate;
	}

	/**
	 * The current state of the audio context: `"suspended"`, `"running"` or
	 * `"closed"`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/state
	 */
	get state(): AudioContextState {
		return _(this).state;
	}

	get audioWorklet(): AudioWorklet {
		throw new Error('Method not implemented.');
	}

	get listener(): AudioListener {
		throw new Error('Method not implemented.');
	}

	/**
	 * Creates a new, empty {@link AudioBuffer}, which can be filled with data
	 * and played via an {@link AudioBufferSourceNode}.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/createBuffer
	 */
	createBuffer(
		numberOfChannels: number,
		length: number,
		sampleRate: number,
	): AudioBuffer {
		return new AudioBuffer({ numberOfChannels, length, sampleRate });
	}

	/**
	 * Creates an {@link AudioBufferSourceNode} for playing an
	 * {@link AudioBuffer}.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/createBufferSource
	 */
	createBufferSource(): AudioBufferSourceNode {
		return new AudioBufferSourceNode(this);
	}

	/**
	 * Creates a {@link GainNode} for controlling volume.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/createGain
	 */
	createGain(): GainNode {
		return new GainNode(this);
	}

	/**
	 * Creates a {@link StereoPannerNode} for panning across the stereo field.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/createStereoPanner
	 */
	createStereoPanner(): StereoPannerNode {
		return new StereoPannerNode(this);
	}

	/**
	 * Asynchronously decodes audio file data contained in an `ArrayBuffer`
	 * into an {@link AudioBuffer}.
	 *
	 * ### Supported Audio Formats
	 *
	 * Any audio container/codec supported by [FFmpeg](https://ffmpeg.org),
	 * including MP3, WAV, OGG Vorbis, Opus, FLAC and AAC/M4A.
	 *
	 * > [!NOTE]
	 * > Unlike browsers, nx.js does NOT resample the decoded data to the
	 * > context's sample rate — the returned buffer keeps the file's native
	 * > rate (resampling happens at playback time).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BaseAudioContext/decodeAudioData
	 */
	decodeAudioData(
		audioData: ArrayBuffer,
		successCallback?: DecodeSuccessCallback | null,
		errorCallback?: DecodeErrorCallback | null,
	): Promise<AudioBuffer> {
		if (!(audioData instanceof ArrayBuffer)) {
			throw new TypeError(
				"Failed to execute 'decodeAudioData' on 'BaseAudioContext': parameter 1 is not of type 'ArrayBuffer'",
			);
		}
		// Per spec, decodeAudioData() detaches the input ArrayBuffer.
		const transfer = (audioData as any).transfer;
		const data: ArrayBuffer =
			typeof transfer === 'function' ? transfer.call(audioData) : audioData;
		const promise = $.audioDecode(data).then(
			({ channelData, sampleRate }) => {
				const buffer = createAudioBuffer(
					channelData.map((ab) => new Float32Array(ab)),
					sampleRate,
				);
				successCallback?.(buffer);
				return buffer;
			},
			(err: unknown) => {
				const error = new DOMException(
					err instanceof Error ? err.message : String(err),
					'EncodingError',
				);
				errorCallback?.(error as unknown as globalThis.DOMException);
				throw error;
			},
		);
		// When a success callback is used, surface failures through the
		// callback path only if an error callback was given.
		return promise;
	}

	createAnalyser(): AnalyserNode {
		throw new Error('Method not implemented.');
	}
	createBiquadFilter(): BiquadFilterNode {
		throw new Error('Method not implemented.');
	}
	createChannelMerger(numberOfInputs?: number): ChannelMergerNode {
		throw new Error('Method not implemented.');
	}
	createChannelSplitter(numberOfOutputs?: number): ChannelSplitterNode {
		throw new Error('Method not implemented.');
	}
	createConstantSource(): ConstantSourceNode {
		throw new Error('Method not implemented.');
	}
	createConvolver(): ConvolverNode {
		throw new Error('Method not implemented.');
	}
	createDelay(maxDelayTime?: number): DelayNode {
		throw new Error('Method not implemented.');
	}
	createDynamicsCompressor(): DynamicsCompressorNode {
		throw new Error('Method not implemented.');
	}
	createIIRFilter(
		feedforward: number[] | Iterable<number>,
		feedback: number[] | Iterable<number>,
	): IIRFilterNode {
		throw new Error('Method not implemented.');
	}
	createOscillator(): OscillatorNode {
		throw new Error('Method not implemented.');
	}
	createPanner(): PannerNode {
		throw new Error('Method not implemented.');
	}
	createPeriodicWave(
		real: number[] | Float32Array | Iterable<number>,
		imag: number[] | Float32Array | Iterable<number>,
		constraints?: PeriodicWaveConstraints,
	): PeriodicWave {
		throw new Error('Method not implemented.');
	}
	createScriptProcessor(
		bufferSize?: number,
		numberOfInputChannels?: number,
		numberOfOutputChannels?: number,
	): ScriptProcessorNode {
		throw new Error('Method not implemented.');
	}
	createWaveShaper(): WaveShaperNode {
		throw new Error('Method not implemented.');
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'statechange') {
			this.onstatechange?.(event);
		}
		return super.dispatchEvent(event);
	}
}
def(BaseAudioContext);
