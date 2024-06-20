import { $ } from '../$';
import { assertInternalConstructor, def, proto } from '../utils';
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
	readonly id!: string;
	readonly index!: number;
	readonly mapping!: GamepadMappingType;
	readonly timestamp!: number;
	readonly vibrationActuator!: GamepadHapticActuator | null;

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

export const gamepads: Gamepad[] = Array(8);

export function gamepadNew(index: number) {
	const g = proto($.gamepadNew(index), Gamepad);
	// @ts-expect-error Readonly prop
	g.buttons = Array(16);
	for (let i = 0; i < 16; i++) {
		// @ts-expect-error Readonly array
		g.buttons[i] = proto($.gamepadButtonNew(g, i), GamepadButton);
	}
	return g;
}
