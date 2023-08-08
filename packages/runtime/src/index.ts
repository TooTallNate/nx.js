import './polyfills';
import { def } from './utils';
import { Switch as _Switch } from './switch';
import { INTERNAL_SYMBOL } from './types';
import { createTimersFactory } from './timers';
import { Console } from './console';
//import { Image } from './image';

export type { Switch } from './switch';

const Switch = new _Switch();
def('Switch', Switch);

def('console', new Console(Switch));

const { setTimeout, setInterval, clearTimeout, clearInterval, processTimers } =
	createTimersFactory();
def('setTimeout', setTimeout);
def('setInterval', setInterval);
def('clearTimeout', clearTimeout);
def('clearInterval', clearInterval);

function touchIsEqual(a: Touch, b: Touch) {
	return (
		a.screenX === b.screenX &&
		a.screenY === b.screenY &&
		a.radiusX === b.radiusX &&
		a.radiusY === b.radiusY &&
		a.rotationAngle === b.rotationAngle
	);
}

const btnPlus = 1 << 10; ///< Plus button

Switch.addEventListener('frame', (event) => {
	const {
		keyboardInitialized,
		touchscreenInitialized,
		previousButtons,
		previousKeys,
		previousTouches,
	} = Switch[INTERNAL_SYMBOL];
	processTimers();

	const buttonsDown = ~previousButtons & event.detail;
	const buttonsUp = previousButtons & ~event.detail;
	Switch[INTERNAL_SYMBOL].previousButtons = event.detail;

	if (buttonsDown !== 0) {
		const ev = new UIEvent('buttondown', {
			cancelable: true,
			detail: buttonsDown,
		});
		Switch.dispatchEvent(ev);
		if (!ev.defaultPrevented && buttonsDown & btnPlus) {
			return Switch.exit();
		}
	}

	if (buttonsUp !== 0) {
		Switch.dispatchEvent(
			new UIEvent('buttonup', {
				detail: buttonsUp,
			})
		);
	}

	if (keyboardInitialized) {
		const keys = Switch.native.hidGetKeyboardStates();
		for (let i = 0; i < 4; i++) {
			const keysDown = ~previousKeys[i] & keys[i];
			const keysUp = previousKeys[i] & ~keys[i];
			for (let k = 0; k < 64; k++) {
				if (keysDown & (1n << (BigInt(k) & 63n))) {
					const o = {
						keyCode: i * 64 + k,
						modifiers: keys.modifiers,
					};
					Switch.dispatchEvent(new KeyboardEvent('keydown', o));
				}
				if (keysUp & (1n << (BigInt(k) & 63n))) {
					const o = {
						keyCode: i * 64 + k,
						modifiers: keys.modifiers,
					};
					Switch.dispatchEvent(new KeyboardEvent('keyup', o));
				}
			}
		}
		Switch[INTERNAL_SYMBOL].previousKeys = keys;
	}

	if (touchscreenInitialized) {
		const touches = Switch.native.hidGetTouchScreenStates();
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

			Switch[INTERNAL_SYMBOL].previousTouches = touches;

			for (const prevTouch of previousTouches) {
				if (!touchIds.has(prevTouch.identifier)) {
					endTouches.push(prevTouch);
				}
			}

			if (startTouches.length) {
				Switch.dispatchEvent(
					new TouchEvent('touchstart', {
						touches,
						changedTouches: startTouches,
					})
				);
			}
			if (changedTouches.length) {
				Switch.dispatchEvent(
					new TouchEvent('touchmove', {
						touches,
						changedTouches,
					})
				);
			}
			if (endTouches.length) {
				Switch.dispatchEvent(
					new TouchEvent('touchend', {
						touches,
						changedTouches: endTouches,
					})
				);
			}
		} else if (previousTouches.length) {
			// No current touches, but there were previous touches, so fire "touchend"
			Switch.dispatchEvent(
				new TouchEvent('touchend', {
					touches: [],
					changedTouches: previousTouches,
				})
			);
			Switch[INTERNAL_SYMBOL].previousTouches = [];
		}
	}
});

Switch.addEventListener('exit', () => {
	Switch[INTERNAL_SYMBOL].cleanup();
});
