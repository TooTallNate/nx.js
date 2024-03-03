import { $ } from './$';
import { TouchEvent } from './polyfills/event';
import type { Screen } from './screen';
import type { Touch } from './polyfills/event';

let init = false;
let previousTouches: Touch[] = [];

export function initTouchscreen() {
	if (!init) {
		$.hidInitializeTouchScreen();
		init = true;
	}
}

export function dispatchTouchEvents(screen: Screen) {
	if (!init) return;
	const touches = $.hidGetTouchScreenStates();
	if (touches) {
		const startTouches: Touch[] = [];
		const changedTouches: Touch[] = [];
		const endTouches: Touch[] = [];
		const touchIds = new Set<number>();

		for (const touch of touches) {
			let started = true;
			for (const prevTouch of previousTouches) {
				if (touch.identifier === (prevTouch.identifier | 0)) {
					started = false;
					// @ts-expect-error
					touch.identifier = prevTouch.identifier;
					touchIds.add(touch.identifier);
					if (!touchIsEqual(touch, prevTouch)) {
						changedTouches.push(touch);
					}
					break;
				}
			}
			if (started) {
				// @ts-expect-error
				touch.identifier += Math.random();
				touchIds.add(touch.identifier);
				startTouches.push(touch);
			}
		}

		previousTouches = touches;

		for (const prevTouch of previousTouches) {
			if (!touchIds.has(prevTouch.identifier)) {
				endTouches.push(prevTouch);
			}
		}

		if (startTouches.length) {
			screen.dispatchEvent(
				new TouchEvent('touchstart', {
					touches,
					changedTouches: startTouches,
				}),
			);
		}
		if (changedTouches.length) {
			screen.dispatchEvent(
				new TouchEvent('touchmove', {
					touches,
					changedTouches,
				}),
			);
		}
		if (endTouches.length) {
			screen.dispatchEvent(
				new TouchEvent('touchend', {
					touches,
					changedTouches: endTouches,
				}),
			);
		}
	} else if (previousTouches.length) {
		// No current touches, but there were previous touches, so fire "touchend"
		screen.dispatchEvent(
			new TouchEvent('touchend', {
				touches: [],
				changedTouches: previousTouches,
			}),
		);
		previousTouches = [];
	}
}

function touchIsEqual(a: Touch, b: Touch) {
	return (
		a.screenX === b.screenX &&
		a.screenY === b.screenY &&
		a.radiusX === b.radiusX &&
		a.radiusY === b.radiusY &&
		a.rotationAngle === b.rotationAngle
	);
}
