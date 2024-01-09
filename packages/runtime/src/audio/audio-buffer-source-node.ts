import { AudioScheduledSourceNode } from './audio-scheduled-source-node';
import type { AudioContext } from './audio-context';
import { INTERNAL_SYMBOL } from '../internal';
import { def } from '../utils';

export class AudioBufferSourceNode
	extends AudioScheduledSourceNode
	implements globalThis.AudioBufferSourceNode
{
	buffer: AudioBuffer | null;
	loop: boolean;
	loopEnd: number;
	loopStart: number;

	constructor(ctx: AudioContext) {
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, ctx);
		this.buffer = null;
		this.loop = false;
		this.loopStart = 0;
		this.loopEnd = 0;
	}

	get detune(): AudioParam {
		throw new Error('Method not implemented.');
	}

	get playbackRate(): AudioParam {
		throw new Error('Method not implemented.');
	}
}
def(AudioBufferSourceNode);
