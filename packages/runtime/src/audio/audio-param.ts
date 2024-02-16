import { assertInternalConstructor, createInternal, def } from '../utils';
import type { AutomationRate } from '../types';

interface AudioParamInternal {
	automationRate: AutomationRate;
	defaultValue: number;
	maxValue: number;
	minValue: number;
	value: number;
}

const _ = createInternal<AudioParam, AudioParamInternal>();

export class AudioParam implements globalThis.AudioParam {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		_.set(this, arguments[1]);
	}
	get automationRate(): AutomationRate {
		return _(this).automationRate;
	}
	get defaultValue(): number {
		return _(this).defaultValue;
	}
	get maxValue(): number {
		return _(this).maxValue;
	}
	get minValue(): number {
		return _(this).minValue;
	}
	get value(): number {
		return _(this).value;
	}
	set value(v: number) {
		_(this).value = v;
	}
	cancelAndHoldAtTime(cancelTime: number): AudioParam {
		throw new Error('Method not implemented.');
	}
	cancelScheduledValues(cancelTime: number): AudioParam {
		throw new Error('Method not implemented.');
	}
	exponentialRampToValueAtTime(value: number, endTime: number): AudioParam {
		throw new Error('Method not implemented.');
	}
	linearRampToValueAtTime(value: number, endTime: number): AudioParam {
		throw new Error('Method not implemented.');
	}
	setTargetAtTime(
		target: number,
		startTime: number,
		timeConstant: number
	): AudioParam {
		throw new Error('Method not implemented.');
	}
	setValueAtTime(value: number, startTime: number): AudioParam {
		throw new Error('Method not implemented.');
	}
	setValueCurveAtTime(
		values: number[] | Float32Array,
		startTime: number,
		duration: number
	): AudioParam;
	setValueCurveAtTime(
		values: Iterable<number>,
		startTime: number,
		duration: number
	): AudioParam;
	setValueCurveAtTime(
		values: unknown,
		startTime: unknown,
		duration: unknown
	): AudioParam {
		throw new Error('Method not implemented.');
	}
}
def(AudioParam);
