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
		super(...arguments);
	}

	get maxChannelCount(): number {
		throw new Error('Method not implemented.');
	}
}
def(AudioDestinationNode);
