import { $ } from './$';
import { KeyboardEvent } from './polyfills/event';
import type { Keys } from './internal';
import type { EventTarget } from './polyfills/event-target';

let init = false;
let previousKeys: Keys = {
	[0]: 0n,
	[1]: 0n,
	[2]: 0n,
	[3]: 0n,
	modifiers: 0n,
};

export function initKeyboard() {
	if (!init) {
		$.hidInitializeKeyboard();
		init = true;
	}
}

export function dispatchKeyboardEvents(target: EventTarget) {
	if (!init) return;
	const keys = $.hidGetKeyboardStates();
	for (let i = 0; i < 4; i++) {
		const keysDown = ~previousKeys[i] & keys[i];
		const keysUp = previousKeys[i] & ~keys[i];
		for (let k = 0; k < 64; k++) {
			if (keysDown & (1n << (BigInt(k) & 63n))) {
				const o = {
					keyCode: i * 64 + k,
					modifiers: keys.modifiers,
				};
				target.dispatchEvent(new KeyboardEvent('keydown', o));
			}
			if (keysUp & (1n << (BigInt(k) & 63n))) {
				const o = {
					keyCode: i * 64 + k,
					modifiers: keys.modifiers,
				};
				target.dispatchEvent(new KeyboardEvent('keyup', o));
			}
		}
	}
	previousKeys = keys;
}
