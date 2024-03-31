import { def } from './utils';
import { performance } from './performance';
import type { DOMHighResTimeStamp } from './types';

export interface FrameRequestCallback {
	(time: DOMHighResTimeStamp): void;
}

const cbs: { id: number; fn: FrameRequestCallback }[] = [];
let rafIndex = 0;

export function callRafCallbacks() {
	const now = performance.now();
	const c = cbs.slice();
	cbs.length = 0;
	for (const cb of c) cb.fn(now);
}

/**
 * Cancels an animation frame request previously scheduled through
 * a call to {@link requestAnimationFrame | `requestAnimationFrame()`}.
 *
 * @param id The ID value returned by the call to {@link requestAnimationFrame | `requestAnimationFrame()`} that requested the callback.
 * @see https://developer.mozilla.org/docs/Web/API/window/cancelAnimationFrame
 */
export function cancelAnimationFrame(id: number): void {
	const index = cbs.findIndex((cb) => cb.id === id);
	if (index !== -1) {
		cbs.splice(index, 1);
	}
}
def(cancelAnimationFrame);

/**
 * Tells the application that you wish to perform an animation. The application
 * will call the supplied callback function prior to the next repaint.
 *
 * @param callback The function to call when it's time to update your animation for the next repaint.
 * @returns The request ID, that uniquely identifies the entry in the callback list. You can pass this value to {@link cancelAnimationFrame | `cancelAnimationFrame()`} to cancel the refresh callback request.
 * @see https://developer.mozilla.org/docs/Web/API/window/requestAnimationFrame
 */
export function requestAnimationFrame(callback: FrameRequestCallback): number {
	const id = rafIndex++;
	cbs.push({ id, fn: callback });
	return id;
}
def(requestAnimationFrame);
