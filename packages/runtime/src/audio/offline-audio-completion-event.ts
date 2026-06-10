import { Event } from '../polyfills/event';
import { def } from '../utils';
import type { AudioBuffer } from './audio-buffer';

export interface OfflineAudioCompletionEventInit extends EventInit {
	renderedBuffer: AudioBuffer;
}

/**
 * The event dispatched on an {@link OfflineAudioContext} when rendering
 * completes.
 *
 * @see https://developer.mozilla.org/docs/Web/API/OfflineAudioCompletionEvent
 */
export class OfflineAudioCompletionEvent extends Event {
	/**
	 * The rendered {@link AudioBuffer}.
	 */
	readonly renderedBuffer: AudioBuffer;

	constructor(type: string, eventInitDict: OfflineAudioCompletionEventInit) {
		super(type, eventInitDict);
		this.renderedBuffer = eventInitDict.renderedBuffer;
	}
}
def(OfflineAudioCompletionEvent);
