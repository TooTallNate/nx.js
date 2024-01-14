import { assertInternalConstructor, createInternal, def } from '../utils';
import { EventTarget } from '../polyfills/event-target';
import { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';
import type { ChannelCountMode, ChannelInterpretation } from '../types';

export interface AudioNodeOptions {
	channelCount?: number;
	channelCountMode?: ChannelCountMode;
	channelInterpretation?: ChannelInterpretation;
}

interface AudioNodeInternal {
	context: BaseAudioContext;
	channelCount: number;
	channelCountMode: ChannelCountMode;
	channelInterpretation: ChannelInterpretation;
}

const _ = createInternal<AudioNode, AudioNodeInternal>();

/**
 * @see https://developer.mozilla.org/docs/Web/API/AudioNode
 */
export class AudioNode extends EventTarget implements globalThis.AudioNode {
	declare channelCount: number;
	declare channelCountMode: ChannelCountMode;
	declare channelInterpretation: ChannelInterpretation;
	declare readonly numberOfInputs: number;
	declare readonly numberOfOutputs: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		const { channelCount, channelCountMode, channelInterpretation } =
			arguments[2];
		_.set(this, {
			context: arguments[1],
			channelCount,
			channelCountMode,
			channelInterpretation,
		});
	}

	/**
	 * The associated {@link BaseAudioContext}, that is the object representing
	 * the processing graph the node is participating in.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/context
	 */
	get context(): BaseAudioContext {
		return _(this).context;
	}

	/**
	 * Connect one of the node's outputs to a target, which may be either another
	 * {@link AudioNode} (thereby directing the sound data to the specified node)
	 * or an {@link AudioParam}, so that the node's output data is automatically
	 * used to change the value of that parameter over time.
	 *
	 * @param destinationNode The `AudioNode` or `AudioParam` to which to connect.
	 * @param output An index specifying which output of the current `AudioNode` to connect to the destination (default: `0`).
	 * @param input An index describing which input of the destination you want to connect the current `AudioNode` to (default: `0`).
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/connect
	 */
	connect(
		destinationNode: AudioNode,
		output?: number,
		input?: number
	): AudioNode;
	connect(destinationParam: AudioParam, output?: number): void;
	connect(
		destinationNode: AudioNode | AudioParam,
		output = 0,
		input = 0
	): void | AudioNode {
		if (destinationNode instanceof AudioParam) {
			throw new Error('AudioParam parameter not implemented.');
		}

		// AudioNode
		return destinationNode;
	}

	disconnect(): void;
	disconnect(output: number): void;
	disconnect(destinationNode: AudioNode): void;
	disconnect(destinationNode: AudioNode, output: number): void;
	disconnect(destinationNode: AudioNode, output: number, input: number): void;
	disconnect(destinationParam: AudioParam): void;
	disconnect(destinationParam: AudioParam, output: number): void;
	disconnect(
		destinationNode?: unknown,
		output?: unknown,
		input?: unknown
	): void {
		throw new Error('Method not implemented.');
	}
}
def(AudioNode);
