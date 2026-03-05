/**
 * Input adapter: maps nx.js gamepad + touch to keyboard/pointer events for Phaser.
 */
// Button indices from @nx.js/constants (inlined to avoid build dep on dist)
const Button = {
	B: 0, A: 1, Y: 2, X: 3,
	L: 4, R: 5, ZL: 6, ZR: 7,
	Minus: 8, Plus: 9, StickL: 10, StickR: 11,
	Up: 12, Down: 13, Left: 14, Right: 15,
} as const;

const SCREEN_W = 1280;
const SCREEN_H = 720;

const keyState: Record<string, boolean> = {};

/**
 * Create a keyboard event object that Phaser can read.
 * We can't use nx.js's native KeyboardEvent constructor because it has a bug
 * where 0n (falsy BigInt) modifier value causes "Failed to get internal state".
 * Instead, create a plain Event and bolt on the KeyboardEvent properties.
 */
function makeKeyEvent(type: string, code: string, key: string): Event {
	const e = new Event(type, { bubbles: true, cancelable: true });
	Object.defineProperties(e, {
		code:      { value: code, enumerable: true },
		key:       { value: key, enumerable: true },
		keyCode:   { value: keyCodeMap[code] ?? 0, enumerable: true },
		which:     { value: keyCodeMap[code] ?? 0, enumerable: true },
		altKey:    { value: false, enumerable: true },
		ctrlKey:   { value: false, enumerable: true },
		shiftKey:  { value: false, enumerable: true },
		metaKey:   { value: false, enumerable: true },
		repeat:    { value: false, enumerable: true },
		location:  { value: 0, enumerable: true },
		charCode:  { value: 0, enumerable: true },
		isComposing: { value: false, enumerable: true },
		getModifierState: { value: () => false, enumerable: true },
		preventDefault: { value: () => {}, writable: true },
	});
	return e;
}

const keyCodeMap: Record<string, number> = {
	ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40,
	Space: 32, Enter: 13, Escape: 27,
};

function dispatchKey(type: 'keydown' | 'keyup', code: string, key: string) {
	const already = keyState[code] ?? false;
	if (type === 'keydown' && already) return; // no repeats
	keyState[code] = type === 'keydown';
	globalThis.dispatchEvent(makeKeyEvent(type, code, key));
}

const buttonMap: [number, string, string][] = [
	[Button.Left, 'ArrowLeft', 'ArrowLeft'],
	[Button.Right, 'ArrowRight', 'ArrowRight'],
	[Button.Up, 'ArrowUp', 'ArrowUp'],
	[Button.Down, 'ArrowDown', 'ArrowDown'],
	[Button.A, 'Space', ' '],
];

const prevButtons: Record<number, boolean> = {};

export function pollGamepad() {
	const gamepads = navigator.getGamepads();
	const gp = gamepads[0];
	if (!gp) return;

	for (const [btn, code, key] of buttonMap) {
		const pressed = gp.buttons[btn]?.pressed ?? false;
		const was = prevButtons[btn] ?? false;
		if (pressed && !was) dispatchKey('keydown', code, key);
		if (!pressed && was) dispatchKey('keyup', code, key);
		prevButtons[btn] = pressed;
	}
}

// Touch → pointer events
export function setupTouchAdapter() {
	screen.addEventListener('touchmove', (e: any) => {
		const touch = e.touches?.[0] ?? e.changedTouches?.[0];
		if (!touch) return;
		globalThis.dispatchEvent(
			new PointerEvent('pointermove', {
				clientX: touch.clientX ?? touch.pageX ?? 0,
				clientY: touch.clientY ?? touch.pageY ?? 0,
				bubbles: true,
			}),
		);
	});

	screen.addEventListener('touchstart', (e: any) => {
		globalThis.dispatchEvent(
			new PointerEvent('pointerdown', { bubbles: true }),
		);
	});

	screen.addEventListener('touchend', () => {
		globalThis.dispatchEvent(
			new PointerEvent('pointerup', { bubbles: true }),
		);
	});
}
