import { Event } from '../polyfills/event';
import { AudioNode } from './audio-node';
import { assertInternalConstructor, def } from '../utils';

export class AudioScheduledSourceNode
	extends AudioNode
	implements globalThis.AudioScheduledSourceNode
{
	onended: ((this: AudioScheduledSourceNode, ev: Event) => any) | null;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		// @ts-expect-error internal constructor
		super(...arguments);
		this.onended = null;
	}

	start(when?: number | undefined): void {
		throw new Error('Method not implemented.');
	}

	stop(when?: number | undefined): void {
		throw new Error('Method not implemented.');
	}
}
def(AudioScheduledSourceNode);
