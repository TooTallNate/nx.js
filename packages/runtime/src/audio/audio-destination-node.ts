import { def } from '../utils';
import { AudioNode } from './audio-node';

/**
 * @see https://developer.mozilla.org/docs/Web/API/AudioDestinationNode
 */
export class AudioDestinationNode
	extends AudioNode
	implements globalThis.AudioDestinationNode
{
	/**
	 * @ignore
	 */
	constructor() {
		// @ts-expect-error internal constructor
		super(arguments[0], arguments[1], {
			numberOfInputs: 1,
			numberOfOutputs: 1,
			channelCount: 2,
			channelCountMode: 'explicit',
			channelInterpretation: 'speakers',
		});
	}

	/**
	 * The maximum amount of channels that the physical device can handle.
	 *
	 * @see https://developer.mozilla.org/en-US/docs/Web/API/AudioDestinationNode/maxChannelCount
	 */
	get maxChannelCount(): number {
		return 2;
	}
}
def(AudioDestinationNode);
