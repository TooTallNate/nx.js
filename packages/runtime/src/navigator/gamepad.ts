import { $ } from '../$';
import { assertInternalConstructor, def, proto } from '../utils';
import { GamepadEvent } from '../polyfills/event';
import type {
	GamepadHapticActuatorType,
	GamepadMappingType,
	GamepadEffectParameters,
} from '../types';

/**
 * Defines an individual gamepad or other controller, allowing access
 * to information such as button presses, axis positions, and id.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Gamepad
 */
export class Gamepad implements globalThis.Gamepad {
	readonly axes!: readonly number[];
	readonly buttons!: readonly GamepadButton[];
	readonly connected!: boolean;
	/**
	 * A string identifying the controller. When available (firmware 5.0.0+),
	 * this is the controller's device name plus its hardware serial number,
	 * e.g. `"Nintendo Switch Pro Controller (XAW10012345678)"`, making it
	 * stable and unique per physical controller.
	 *
	 * The serial is read from the `hid:sys` service and cached — it is only
	 * re-queried when a controller is connected or disconnected, never on the
	 * input-polling hot path. When the serial can't be obtained (older
	 * firmware, no serial, or a system error) `id` falls back to a
	 * unique-per-slot string of the form `"switch-gamepad-<index>"`.
	 */
	readonly id!: string;
	readonly index!: number;
	readonly mapping!: GamepadMappingType;
	readonly timestamp!: number;
	readonly vibrationActuator!: GamepadHapticActuator;

	// Non-standard
	readonly deviceType!: number;
	readonly rawButtons!: bigint;
	readonly styleSet!: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}
}
def(Gamepad);
$.gamepadInit(Gamepad);

/**
 * Defines an individual button of a gamepad or other controller, allowing access
 * to the current state of different types of buttons available on the control device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/GamepadButton
 */
export class GamepadButton implements globalThis.GamepadButton {
	pressed!: boolean;
	touched!: boolean;
	value!: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}
}
def(GamepadButton);
$.gamepadButtonInit(GamepadButton);

/**
 * Represents hardware in the controller designed to provide haptic feedback
 * to the user (if available), most commonly vibration hardware.
 *
 * @see https://developer.mozilla.org/docs/Web/API/GamepadHapticActuator
 */
export class GamepadHapticActuator implements globalThis.GamepadHapticActuator {
	readonly type!: GamepadHapticActuatorType;

	playEffect(
		type: 'dual-rumble',
		params?: GamepadEffectParameters,
	): Promise<GamepadHapticsResult> {
		throw new Error('Method not implemented.');
	}

	reset(): Promise<GamepadHapticsResult> {
		throw new Error('Method not implemented.');
	}

	pulse(duration: number, delay?: number): void {
		throw new Error('Method not implemented.');
	}
}
def(GamepadHapticActuator);

export function gamepadNew(index: number) {
	const g = proto($.gamepadNew(index), Gamepad);
	// @ts-expect-error Readonly prop
	g.mapping = 'standard';
	// @ts-expect-error Readonly prop
	g.buttons = Array(16);
	for (let i = 0; i < 16; i++) {
		// @ts-expect-error Readonly array
		g.buttons[i] = proto($.gamepadButtonNew(g, i), GamepadButton);
	}
	return g;
}

export const gamepads: Gamepad[] = Array(8)
	.fill(0)
	.map((_, i) => gamepadNew(i));

// Tracks the last-seen connection state of each slot so we can diff and emit
// `gamepadconnected` / `gamepaddisconnected` only on an actual transition.
const connectedState: boolean[] = Array(8).fill(false);

/**
 * Detects controller connect/disconnect transitions and returns the
 * `GamepadEvent`s that should be dispatched (in order). Cheap on the common
 * path: it only diffs the 8 slots when the OS connection event has fired (the
 * native `$.gamepadConnectionChanged()` is a non-blocking event check, and is
 * also what invalidates the cached `Gamepad.id` values). Returns an empty
 * array when nothing changed.
 *
 * @ignore
 */
export function sweepGamepadConnections(): GamepadEvent[] {
	if (!$.gamepadConnectionChanged()) return [];
	const events: GamepadEvent[] = [];
	for (let i = 0; i < 8; i++) {
		const g = gamepads[i];
		const connected = g.connected;
		if (connected === connectedState[i]) continue;
		connectedState[i] = connected;
		events.push(
			new GamepadEvent(
				connected ? 'gamepadconnected' : 'gamepaddisconnected',
				{ gamepad: g },
			),
		);
	}
	return events;
}
