import { assertInternalConstructor, def } from '../utils';
import { AudioNode } from './audio-node';

export class AudioScheduledSourceNode
	extends AudioNode
	implements globalThis.AudioScheduledSourceNode
{
	onended: ((this: AudioScheduledSourceNode, ev: Event) => any) | null;
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
