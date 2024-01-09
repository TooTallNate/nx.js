import { INTERNAL_SYMBOL } from '../internal';
import { def } from '../utils';
import { BaseAudioContext } from './base-audio-context';

export class AudioContext
	extends BaseAudioContext
	implements globalThis.AudioContext
{
	baseLatency: number;
	outputLatency: number;
	constructor() {
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL);
		this.baseLatency = 0;
		this.outputLatency = 0;
	}
	close(): Promise<void> {
		throw new Error('Method not implemented.');
	}
	createMediaElementSource(
		mediaElement: HTMLMediaElement
	): MediaElementAudioSourceNode {
		throw new Error('Method not implemented.');
	}
	createMediaStreamDestination(): MediaStreamAudioDestinationNode {
		throw new Error('Method not implemented.');
	}
	createMediaStreamSource(
		mediaStream: MediaStream
	): MediaStreamAudioSourceNode {
		throw new Error('Method not implemented.');
	}
	getOutputTimestamp(): AudioTimestamp {
		throw new Error('Method not implemented.');
	}
	resume(): Promise<void> {
		throw new Error('Method not implemented.');
	}
	suspend(): Promise<void> {
		throw new Error('Method not implemented.');
	}
}
def(AudioContext);
