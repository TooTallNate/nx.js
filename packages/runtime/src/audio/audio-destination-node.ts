import { assertInternalConstructor, def } from '../utils';
import { AudioNode } from './audio-node';

/**
 * The end destination of an audio graph — the audio output device of the
 * console. Obtained via {@link BaseAudioContext.destination | `ctx.destination`};
 * not user-constructible.
 *
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
		assertInternalConstructor(arguments);
		// @ts-expect-error internal constructor
		super(...arguments);
	}

	/**
	 * The maximum number of channels that the physical device can handle.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioDestinationNode/maxChannelCount
	 */
	get maxChannelCount(): number {
		return 2;
	}
}
def(AudioDestinationNode);
