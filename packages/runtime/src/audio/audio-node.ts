import { assertInternalConstructor, createInternal, def } from '../utils';
import { EventTarget } from '../polyfills/event-target';
import type { BaseAudioContext } from './base-audio-context';

interface AudioNodeInternal {
	context: BaseAudioContext;
}

const _ = createInternal<AudioNode, AudioNodeInternal>();

export class AudioNode extends EventTarget implements globalThis.AudioNode {
	declare channelCount: number;
	declare channelCountMode: ChannelCountMode;
	declare channelInterpretation: ChannelInterpretation;
	declare readonly context: BaseAudioContext;
	declare readonly numberOfInputs: number;
	declare readonly numberOfOutputs: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		_.set(this, {
			context: arguments[1],
		});
	}
	connect(
		destinationNode: AudioNode,
		output?: number | undefined,
		input?: number | undefined
	): AudioNode;
	connect(destinationParam: AudioParam, output?: number | undefined): void;
	connect(
		destinationNode: unknown,
		output?: unknown,
		input?: unknown
	): void | AudioNode {
		throw new Error('Method not implemented.');
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
