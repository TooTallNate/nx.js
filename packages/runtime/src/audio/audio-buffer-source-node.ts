import { $ } from '../$';
import { DOMException } from '../dom-exception';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import type { AudioBuffer } from './audio-buffer';
import { createAudioParam } from './audio-param';
import {
	AudioScheduledSourceNode,
	trackActiveSource,
} from './audio-scheduled-source-node';
import {
	bufferInternal,
	ctxInternal,
	NODE_TYPE_BUFFER_SOURCE,
	nodeInternal,
} from './internal';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface AudioBufferSourceOptions {
	buffer?: AudioBuffer | null;
	detune?: number;
	loop?: boolean;
	loopEnd?: number;
	loopStart?: number;
	playbackRate?: number;
}

interface AudioBufferSourceNodeInternal {
	buffer: AudioBuffer | null;
	started: boolean;
	loop: boolean;
	loopStart: number;
	loopEnd: number;
	playbackRate: AudioParam;
	detune: AudioParam;
}

const _ = createInternal<AudioBufferSourceNode, AudioBufferSourceNodeInternal>();

function checkScheduleArg(name: string, paramName: string, v: number) {
	if (!Number.isFinite(v)) {
		throw new TypeError(
			`Failed to execute '${name}' on 'AudioBufferSourceNode': The provided ${paramName} is non-finite.`,
		);
	}
	if (v < 0) {
		throw new RangeError(
			`Failed to execute '${name}' on 'AudioBufferSourceNode': The ${paramName} provided (${v}) cannot be negative.`,
		);
	}
}

function syncLoop(node: AudioBufferSourceNode) {
	const i = _(node);
	$.audioSourceSetLoop(
		nodeInternal(node).handle,
		i.loop,
		i.loopStart,
		i.loopEnd,
	);
}

// Hand the buffer's channel data to the native node. The render thread reads
// the Float32Arrays' backing stores directly (no copy).
function syncBuffer(node: AudioBufferSourceNode) {
	const i = _(node);
	if (!i.buffer) return;
	const b = bufferInternal(i.buffer);
	$.audioSourceSetBuffer(
		nodeInternal(node).handle,
		b.channels,
		b.length,
		b.sampleRate,
	);
}

/**
 * An audio source consisting of in-memory audio data, stored in an
 * {@link AudioBuffer}. This is the node to use when playing back one-shot or
 * looping sounds (e.g. game sound effects and music).
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode
 */
export class AudioBufferSourceNode
	extends AudioScheduledSourceNode
	implements globalThis.AudioBufferSourceNode
{
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/AudioBufferSourceNode
	 */
	constructor(context: BaseAudioContext, options: AudioBufferSourceOptions = {}) {
		const handle = $.audioNodeNew(
			ctxInternal(context).handle,
			NODE_TYPE_BUFFER_SOURCE,
		);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			context,
			handle,
			numberOfInputs: 0,
			numberOfOutputs: 1,
			channelCount: 2,
			channelCountMode: 'max',
			channelInterpretation: 'speakers',
		});
		_.set(this, {
			buffer: null,
			started: false,
			loop: options.loop ?? false,
			loopStart: options.loopStart ?? 0,
			loopEnd: options.loopEnd ?? 0,
			playbackRate: createAudioParam(this, handle, 0, {
				defaultValue: 1,
			}),
			detune: createAudioParam(this, handle, 1, {
				defaultValue: 0,
			}),
		});
		const i = _(this);
		if (typeof options.playbackRate === 'number') {
			i.playbackRate.value = options.playbackRate;
		}
		if (typeof options.detune === 'number') {
			i.detune.value = options.detune;
		}
		if (i.loop || i.loopStart || i.loopEnd) syncLoop(this);
		if (options.buffer) this.buffer = options.buffer;
	}

	/**
	 * The {@link AudioBuffer} providing the audio asset to play.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/buffer
	 */
	get buffer(): AudioBuffer | null {
		return _(this).buffer;
	}

	set buffer(buffer: AudioBuffer | null) {
		const i = _(this);
		if (buffer && i.buffer) {
			throw new DOMException(
				"Failed to set the 'buffer' property on 'AudioBufferSourceNode': Cannot set buffer to non-null after it has been already been set to a non-null buffer",
				'InvalidStateError',
			);
		}
		i.buffer = buffer;
		if (buffer && i.started) syncBuffer(this);
	}

	/**
	 * Speed factor at which the audio asset will be played (`1` = normal).
	 * Since no pitch correction is applied, this changes the pitch of the
	 * sound as well.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/playbackRate
	 */
	get playbackRate(): AudioParam {
		return _(this).playbackRate;
	}

	/**
	 * Detuning of the playback, in cents.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/detune
	 */
	get detune(): AudioParam {
		return _(this).detune;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/loop
	 */
	get loop(): boolean {
		return _(this).loop;
	}

	set loop(v: boolean) {
		_(this).loop = v;
		syncLoop(this);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/loopStart
	 */
	get loopStart(): number {
		return _(this).loopStart;
	}

	set loopStart(v: number) {
		_(this).loopStart = v;
		syncLoop(this);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/loopEnd
	 */
	get loopEnd(): number {
		return _(this).loopEnd;
	}

	set loopEnd(v: number) {
		_(this).loopEnd = v;
		syncLoop(this);
	}

	/**
	 * Schedules the buffer to begin playback.
	 *
	 * @param when Time (in context seconds) the playback should begin. Values in the past begin immediately.
	 * @param offset Offset (in seconds) into the buffer where playback should begin.
	 * @param duration Duration (in seconds) of buffer content to play.
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/start
	 */
	start(when = 0, offset = 0, duration?: number): void {
		checkScheduleArg('start', 'start time', when);
		checkScheduleArg('start', 'offset', offset);
		if (typeof duration === 'number') {
			checkScheduleArg('start', 'duration', duration);
		}
		const i = _(this);
		if (i.started) {
			throw new DOMException(
				"Failed to execute 'start' on 'AudioBufferSourceNode': cannot call start more than once.",
				'InvalidStateError',
			);
		}
		i.started = true;
		syncBuffer(this);
		$.audioSourceStart(
			nodeInternal(this).handle,
			when,
			offset,
			typeof duration === 'number' ? duration : -1,
		);
		trackActiveSource(this);
	}

	/**
	 * Schedules the playback to stop.
	 *
	 * @param when Time (in context seconds) the playback should stop. Values in the past stop immediately.
	 * @see https://developer.mozilla.org/docs/Web/API/AudioBufferSourceNode/stop
	 */
	stop(when = 0): void {
		checkScheduleArg('stop', 'stop time', when);
		const i = _(this);
		if (!i.started) {
			throw new DOMException(
				"Failed to execute 'stop' on 'AudioBufferSourceNode': cannot call stop without calling start first.",
				'InvalidStateError',
			);
		}
		$.audioSourceStop(nodeInternal(this).handle, when);
	}
}
def(AudioBufferSourceNode);
