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

function dispatchKey(type: 'keydown' | 'keyup', code: string, key: string) {
	const already = keyState[code] ?? false;
	if (type === 'keydown' && already) return; // no repeats
	keyState[code] = type === 'keydown';
	globalThis.dispatchEvent(
		new KeyboardEvent(type, {
			code,
			key,
			bubbles: true,
			cancelable: true,
		}),
	);
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
