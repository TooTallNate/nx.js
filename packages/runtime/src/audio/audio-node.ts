import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { EventTarget } from '../polyfills/event-target';
import { assertInternalConstructor, def } from '../utils';
import { AudioParam } from './audio-param';
import { nodeInternal as _, type AudioNodeInternal } from './internal';
import type { BaseAudioContext } from './base-audio-context';

export interface AudioNodeOptions {
	channelCount?: number;
	channelCountMode?: ChannelCountMode;
	channelInterpretation?: ChannelInterpretation;
}

/**
 * A generic interface for representing an audio processing module, the
 * building block of an audio routing graph.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioNode
 */
export class AudioNode extends EventTarget implements globalThis.AudioNode {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		_.set(this, arguments[1] as AudioNodeInternal);
	}

	/**
	 * The {@link BaseAudioContext} which the node participates in.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/context
	 */
	get context(): BaseAudioContext {
		return _(this).context;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/numberOfInputs
	 */
	get numberOfInputs(): number {
		return _(this).numberOfInputs;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/numberOfOutputs
	 */
	get numberOfOutputs(): number {
		return _(this).numberOfOutputs;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/channelCount
	 */
	get channelCount(): number {
		return _(this).channelCount;
	}

	set channelCount(v: number) {
		if (!Number.isInteger(v) || v < 1 || v > 32) {
			throw new DOMException(
				`The channel count provided (${v}) is outside the range [1, 32]`,
				'NotSupportedError',
			);
		}
		_(this).channelCount = v;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/channelCountMode
	 */
	get channelCountMode(): ChannelCountMode {
		return _(this).channelCountMode;
	}

	set channelCountMode(v: ChannelCountMode) {
		_(this).channelCountMode = v;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/channelInterpretation
	 */
	get channelInterpretation(): ChannelInterpretation {
		return _(this).channelInterpretation;
	}

	set channelInterpretation(v: ChannelInterpretation) {
		_(this).channelInterpretation = v;
	}

	/**
	 * Connects one of the node's outputs to a destination node, directing the
	 * processed audio data to it.
	 *
	 * > [!NOTE]
	 * > Connecting to an `AudioParam` is not currently supported in nx.js.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/connect
	 */
	connect(
		destinationNode: AudioNode,
		output?: number,
		input?: number,
	): AudioNode;
	connect(destinationParam: AudioParam, output?: number): void;
	connect(
		destination: AudioNode | AudioParam,
		output = 0,
		input = 0,
	): AudioNode | void {
		const i = _(this);
		if (destination instanceof AudioParam) {
			throw new DOMException(
				"Failed to execute 'connect' on 'AudioNode': AudioParam connections are not supported",
				'InvalidAccessError',
			);
		}
		if (!(destination instanceof AudioNode)) {
			throw new TypeError(
				"Failed to execute 'connect' on 'AudioNode': parameter 1 is not of type 'AudioNode'",
			);
		}
		const d = _(destination);
		if (d.context !== i.context) {
			throw new DOMException(
				"Failed to execute 'connect' on 'AudioNode': cannot connect to an AudioNode belonging to a different audio context",
				'InvalidAccessError',
			);
		}
		if (output < 0 || output >= i.numberOfOutputs) {
			throw new DOMException(
				`Failed to execute 'connect' on 'AudioNode': output index (${output}) exceeds number of outputs (${i.numberOfOutputs})`,
				'IndexSizeError',
			);
		}
		if (input < 0 || input >= d.numberOfInputs) {
			throw new DOMException(
				`Failed to execute 'connect' on 'AudioNode': input index (${input}) exceeds number of inputs (${d.numberOfInputs})`,
				'IndexSizeError',
			);
		}
		$.audioNodeConnect(i.handle, d.handle);
		return destination;
	}

	/**
	 * Disconnects one or more nodes from the node.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioNode/disconnect
	 */
	disconnect(): void;
	disconnect(output: number): void;
	disconnect(destinationNode: AudioNode): void;
	disconnect(destinationNode: AudioNode, output: number): void;
	disconnect(destinationNode: AudioNode, output: number, input: number): void;
	disconnect(destinationParam: AudioParam): void;
	disconnect(destinationParam: AudioParam, output: number): void;
	disconnect(destination?: AudioNode | AudioParam | number): void {
		const i = _(this);
		if (destination instanceof AudioParam) {
			throw new DOMException(
				"Failed to execute 'disconnect' on 'AudioNode': AudioParam connections are not supported",
				'InvalidAccessError',
			);
		}
		if (destination instanceof AudioNode) {
			$.audioNodeDisconnect(i.handle, _(destination).handle);
		} else {
			// No argument (or an output index — all of our nodes have a
			// single output, so it addresses everything).
			if (typeof destination === 'number' && destination !== 0) {
				throw new DOMException(
					`Failed to execute 'disconnect' on 'AudioNode': output index (${destination}) exceeds number of outputs (${i.numberOfOutputs})`,
					'IndexSizeError',
				);
			}
			$.audioNodeDisconnect(i.handle);
		}
	}
}
def(AudioNode);
