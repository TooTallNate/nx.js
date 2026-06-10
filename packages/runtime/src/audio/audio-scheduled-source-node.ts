import { $ } from '../$';
import { Event } from '../polyfills/event';
import { clearInterval, setInterval } from '../timers';
import { assertInternalConstructor, def } from '../utils';
import { AudioNode } from './audio-node';
import { nodeInternal, SOURCE_STATE_FINISHED } from './internal';

// Sources which have been start()ed but have not yet finished. The strong
// references here implement the spec's "playing reference" GC protection (an
// actively playing source must not be collected, so that `ended` can fire),
// and the polling interval is how native playback completion is surfaced to
// JS as the `ended` event.
const active = new Set<AudioScheduledSourceNode>();
let pollTimer: ReturnType<typeof setInterval> | null = null;

function poll() {
	for (const node of active) {
		const state = $.audioSourceState(nodeInternal(node).handle);
		if (state === SOURCE_STATE_FINISHED) {
			active.delete(node);
			node.dispatchEvent(new Event('ended'));
		}
	}
	if (active.size === 0 && pollTimer !== null) {
		clearInterval(pollTimer);
		pollTimer = null;
	}
}

/**
 * @internal
 */
export function trackActiveSource(node: AudioScheduledSourceNode) {
	active.add(node);
	if (pollTimer === null) {
		pollTimer = setInterval(poll, 50);
	}
}

/**
 * Parent interface for several types of audio source node. Provides the
 * `start()` / `stop()` scheduling methods and the `ended` event.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioScheduledSourceNode
 */
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

	start(when?: number): void {
		throw new Error('Method not implemented.');
	}

	stop(when?: number): void {
		throw new Error('Method not implemented.');
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'ended') {
			this.onended?.(event);
		}
		return super.dispatchEvent(event);
	}
}
def(AudioScheduledSourceNode);
