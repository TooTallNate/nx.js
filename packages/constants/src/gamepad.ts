/**
 * Mapping of the button indices on a `Gamepad` to the
 * corresponding name of the button on the controller.
 *
 * The order is **not** the standard Web Gamepad API order — it follows the
 * order of buttons as reported by the Switch's HID service. Always use this
 * enum (or named members) to look up buttons in `Gamepad.buttons[]` rather
 * than hard-coding indices.
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
	/** B button (right-side face button on Pro/Joy-Con grip — _down_ position). */
	B,
	/** A button (right-side face button — _right_ position). */
	A,
	/** Y button (left-side face button — _left_ position). */
	Y,
	/** X button (left-side face button — _up_ position). */
	X,
	/** L shoulder button. */
	L,
	/** R shoulder button. */
	R,
	/** ZL trigger. */
	ZL,
	/** ZR trigger. */
	ZR,
	/** Minus (–) button. */
	Minus,
	/** Plus (+) button. */
	Plus,
	/** Click of the Left Stick. */
	StickL,
	/** Click of the Right Stick. */
	StickR,
	/** D-Pad Up. */
	Up,
	/** D-Pad Down. */
	Down,
	/** D-Pad Left. */
	Left,
	/** D-Pad Right. */
	Right,
}
