import { def } from '../utils';
import type { Event } from './event';
import type { AbortSignal } from './abort-controller';

export interface EventListener<T extends Event> {
	(evt: T): void;
}

export interface EventListenerObject<T extends Event> {
	handleEvent(evt: T): void;
}

export type EventListenerOrEventListenerObject<T extends Event = any> =
	| EventListener<T>
	| EventListenerObject<T>;

export interface EventListenerOptions {
	capture?: boolean;
}

export interface AddEventListenerOptions extends EventListenerOptions {
	once?: boolean;
	passive?: boolean;
	signal?: AbortSignal;
}

interface Callback {
	target: EventTarget;
	cb: EventListenerOrEventListenerObject;
	options?: AddEventListenerOptions;
}

const callbacks = new WeakMap<EventTarget, Map<string, Callback[]>>();

const getCbs = (t: EventTarget, type: string): Callback[] => {
	let map = callbacks.get(t);
	if (!map) {
		map = new Map();
		callbacks.set(t, map);
	}
	let cbs = map.get(type);
	if (!cbs) {
		cbs = [];
		map.set(type, cbs);
	}
	return cbs;
};

/**
 * EventTarget is a DOM interface implemented by objects that can receive events and may have listeners for them.
 *
 * @see https://developer.mozilla.org/docs/Web/API/EventTarget
 */
export class EventTarget {
	/**
	 * Appends an event listener for events whose type attribute value is type. The callback argument sets the callback that will be invoked when the event is dispatched.
	 *
	 * The options argument sets listener-specific options. For compatibility this can be a boolean, in which case the method behaves exactly as if the value was specified as options's capture.
	 *
	 * When set to true, options's capture prevents callback from being invoked when the event's eventPhase attribute value is BUBBLING_PHASE. When false (or not present), callback will not be invoked when event's eventPhase attribute value is CAPTURING_PHASE. Either way, callback will be invoked if event's eventPhase attribute value is AT_TARGET.
	 *
	 * When set to true, options's passive indicates that the callback will not cancel the event by invoking preventDefault(). This is used to enable performance optimizations described in § 2.8 Observing event listeners.
	 *
	 * When set to true, options's once indicates that the callback will only be invoked once after which the event listener will be removed.
	 *
	 * If an AbortSignal is passed for options's signal, then the event listener will be removed when signal is aborted.
	 *
	 * The event listener is appended to target's event listener list and is not appended if it has the same type, callback, and capture.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/EventTarget/addEventListener
	 */
	addEventListener(
		type: string,
		callback: EventListenerOrEventListenerObject | null,
		options?: AddEventListenerOptions | boolean,
	): void {
		if (!callback) return;
		if (typeof options === 'boolean') {
			options = { capture: options };
		}
		const self = this || globalThis;
		const cbs = getCbs(self, type);
		if (cbs.some((cb) => cb.cb === callback)) return;
		cbs.push({ target: this || globalThis, cb: callback, options });
	}

	/**
	 * Dispatches a synthetic event event to target and returns true if either event's cancelable attribute value is false or its `preventDefault()` method was not invoked, and false otherwise.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/EventTarget/dispatchEvent
	 */
	dispatchEvent(event: Event): boolean {
		const self = this || globalThis;
		const cbs = getCbs(self, event.type);
		// @ts-expect-error readonly
		event.target = event.currentTarget = self;
		for (const cb of cbs.slice()) {
			if (cb.options?.once) {
				self.removeEventListener(event.type, cb.cb);
			}
			if (typeof cb.cb === 'function') {
				cb.cb(event);
			} else {
				cb.cb.handleEvent(event);
			}
		}
		// @ts-expect-error readonly
		delete event.target;
		// @ts-expect-error readonly
		delete event.currentTarget;
		return !event.defaultPrevented;
	}

	/**
	 * Removes the event listener in target's event listener list with the same type, callback, and options.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/EventTarget/removeEventListener
	 */
	removeEventListener(
		type: string,
		callback: EventListenerOrEventListenerObject | null,
		options?: EventListenerOptions | boolean,
	): void {
		const self = this || globalThis;
		const cbs = getCbs(self, type);
		for (let i = 0; i < cbs.length; i++) {
			if (cbs[i].cb === callback) {
				cbs.splice(i, 1);
				return;
			}
		}
	}
}
def(EventTarget);
