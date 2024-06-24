/**
 * Mapping of the button indicies on a `Gamepad` to the
 * corresponding name of the button on the controller.
 *
 * @example
 *
 * ```typescript
 * import { Button } from '@nx.js/constants';
 *
 * const pad = navigator.getGamepads()[0];
 * if (pad.buttons[Button.A].pressed) {
 *   console.log('Button A is pressed');
 * }
 * ```
 */
export enum Button {
	B,
	A,
	Y,
	X,
	L,
	R,
	ZL,
	ZR,
	Minus,
	Plus,
	StickL,
	StickR,
	Up,
	Down,
	Left,
	Right,
}
