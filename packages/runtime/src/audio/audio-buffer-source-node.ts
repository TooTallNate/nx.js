import { AudioScheduledSourceNode } from './audio-scheduled-source-node';
import { INTERNAL_SYMBOL } from '../internal';
import { def } from '../utils';
import { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface AudioBufferSourceOptions {
	buffer?: AudioBuffer | null;
	detune?: number;
	loop?: boolean;
	loopEnd?: number;
	loopStart?: number;
	playbackRate?: number;
}

/**
 *
 */
export class AudioBufferSourceNode
	extends AudioScheduledSourceNode
	implements globalThis.AudioBufferSourceNode
{
	buffer: AudioBuffer | null;
	loop: boolean;
	loopEnd: number;
	loopStart: number;

	constructor(ctx: BaseAudioContext, opts: AudioBufferSourceOptions = {}) {
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, ctx, {
			numberOfInputs: 0,
			numberOfOutputs: 1,
			channelCount: 2,
			channelCountMode: 'max',
			channelInterpretation: 'speakers',
		});
		this.buffer = opts.buffer || null;
		this.loop = opts.loop || false;
		this.loopStart = opts.loopStart || 0;
		this.loopEnd = opts.loopEnd || 0;
	}

	get detune(): AudioParam {
		throw new Error('Method not implemented.');
	}

	get playbackRate(): AudioParam {
		throw new Error('Method not implemented.');
	}
}
def(AudioBufferSourceNode);
