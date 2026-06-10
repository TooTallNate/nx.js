import { $, type AudioNodeHandle } from '../$';
import { DOMException } from '../dom-exception';
import { assertInternalConstructor, def } from '../utils';
import {
	MOST_POSITIVE_SINGLE,
	PARAM_EVENT_EXPONENTIAL_RAMP,
	PARAM_EVENT_LINEAR_RAMP,
	PARAM_EVENT_SET_TARGET,
	PARAM_EVENT_SET_VALUE,
	paramInternal as _,
} from './internal';
import type { AudioNode } from './audio-node';

function checkFinite(name: string, paramName: string, v: number): number {
	if (!Number.isFinite(v)) {
		throw new TypeError(
			`Failed to execute '${name}' on 'AudioParam': The provided ${paramName} is non-finite.`,
		);
	}
	return v;
}

function checkTime(name: string, paramName: string, t: number): number {
	checkFinite(name, paramName, t);
	if (t < 0) {
		throw new RangeError(
			`Failed to execute '${name}' on 'AudioParam': The ${paramName} provided (${t}) cannot be negative.`,
		);
	}
	return t;
}

/**
 * Create an `AudioParam` bound to native param storage, addressed as
 * (node handle, param index).
 *
 * @internal
 */
export function createAudioParam(
	node: AudioNode,
	handle: AudioNodeHandle,
	index: number,
	opts: {
		defaultValue: number;
		minValue?: number;
		maxValue?: number;
		automationRate?: AutomationRate;
	},
): AudioParam {
	const param = Object.create(AudioParam.prototype) as AudioParam;
	_.set(param, {
		node,
		handle,
		index,
		defaultValue: opts.defaultValue,
		minValue: opts.minValue ?? -MOST_POSITIVE_SINGLE,
		maxValue: opts.maxValue ?? MOST_POSITIVE_SINGLE,
		automationRate: opts.automationRate ?? 'a-rate',
	});
	return param;
}

/**
 * An audio-related parameter of an {@link AudioNode}, which can be set to a
 * specific value or scheduled to change over time with sample-accurate
 * automation.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AudioParam
 */
export class AudioParam implements globalThis.AudioParam {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/automationRate
	 */
	get automationRate(): AutomationRate {
		return _(this).automationRate;
	}

	set automationRate(v: AutomationRate) {
		// The assignment is reflected by the getter, but nx.js params always
		// process at their natural rate (gain/pan a-rate; playbackRate/detune
		// k-rate), so this does not change processing.
		_(this).automationRate = v;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/defaultValue
	 */
	get defaultValue(): number {
		return _(this).defaultValue;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/minValue
	 */
	get minValue(): number {
		return _(this).minValue;
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/maxValue
	 */
	get maxValue(): number {
		return _(this).maxValue;
	}

	/**
	 * The current value of the parameter, taking any scheduled automation
	 * into account.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/value
	 */
	get value(): number {
		const i = _(this);
		return $.audioParamValue(i.handle, i.index);
	}

	set value(v: number) {
		const i = _(this);
		$.audioParamSetValue(i.handle, i.index, v);
	}

	/**
	 * Schedules an instant change of the parameter value at a precise time.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/setValueAtTime
	 */
	setValueAtTime(value: number, startTime: number): AudioParam {
		checkFinite('setValueAtTime', 'value', value);
		checkTime('setValueAtTime', 'startTime', startTime);
		const i = _(this);
		$.audioParamSchedule(
			i.handle,
			i.index,
			PARAM_EVENT_SET_VALUE,
			startTime,
			value,
			0,
		);
		return this;
	}

	/**
	 * Schedules a gradual linear change of the parameter value, ramping from
	 * the previous event to `value` at `endTime`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/linearRampToValueAtTime
	 */
	linearRampToValueAtTime(value: number, endTime: number): AudioParam {
		checkFinite('linearRampToValueAtTime', 'value', value);
		checkTime('linearRampToValueAtTime', 'endTime', endTime);
		const i = _(this);
		$.audioParamSchedule(
			i.handle,
			i.index,
			PARAM_EVENT_LINEAR_RAMP,
			endTime,
			value,
			0,
		);
		return this;
	}

	/**
	 * Schedules a gradual exponential change of the parameter value, ramping
	 * from the previous event to `value` at `endTime`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/exponentialRampToValueAtTime
	 */
	exponentialRampToValueAtTime(value: number, endTime: number): AudioParam {
		checkFinite('exponentialRampToValueAtTime', 'value', value);
		if (value === 0) {
			throw new RangeError(
				"Failed to execute 'exponentialRampToValueAtTime' on 'AudioParam': The float target value provided (0) should not be in the range (-1.40130e-45, 1.40130e-45).",
			);
		}
		checkTime('exponentialRampToValueAtTime', 'endTime', endTime);
		const i = _(this);
		$.audioParamSchedule(
			i.handle,
			i.index,
			PARAM_EVENT_EXPONENTIAL_RAMP,
			endTime,
			value,
			0,
		);
		return this;
	}

	/**
	 * Schedules the start of an exponential approach to the `target` value
	 * starting at `startTime`, decaying with `timeConstant`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/setTargetAtTime
	 */
	setTargetAtTime(
		target: number,
		startTime: number,
		timeConstant: number,
	): AudioParam {
		checkFinite('setTargetAtTime', 'target', target);
		checkTime('setTargetAtTime', 'startTime', startTime);
		checkFinite('setTargetAtTime', 'timeConstant', timeConstant);
		if (timeConstant < 0) {
			throw new RangeError(
				`Failed to execute 'setTargetAtTime' on 'AudioParam': The time constant provided (${timeConstant}) cannot be negative.`,
			);
		}
		const i = _(this);
		$.audioParamSchedule(
			i.handle,
			i.index,
			PARAM_EVENT_SET_TARGET,
			startTime,
			target,
			timeConstant,
		);
		return this;
	}

	/**
	 * Schedules the parameter value to follow a curve of values, scaled to fit
	 * into the interval `[startTime, startTime + duration]`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/setValueCurveAtTime
	 */
	setValueCurveAtTime(
		values: number[] | Float32Array | Iterable<number>,
		startTime: number,
		duration: number,
	): AudioParam {
		const curve =
			values instanceof Float32Array
				? values.slice()
				: new Float32Array(values as Iterable<number>);
		if (curve.length < 2) {
			throw new DOMException(
				`Failed to execute 'setValueCurveAtTime' on 'AudioParam': The curve length provided (${curve.length}) is less than the minimum bound (2).`,
				'InvalidStateError',
			);
		}
		checkTime('setValueCurveAtTime', 'startTime', startTime);
		checkFinite('setValueCurveAtTime', 'duration', duration);
		if (duration <= 0) {
			throw new RangeError(
				`Failed to execute 'setValueCurveAtTime' on 'AudioParam': The curve duration provided (${duration}) should be strictly positive.`,
			);
		}
		const i = _(this);
		$.audioParamSetValueCurve(i.handle, i.index, curve, startTime, duration);
		return this;
	}

	/**
	 * Cancels all scheduled future changes to the parameter at or after
	 * `cancelTime`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AudioParam/cancelScheduledValues
	 */
	cancelScheduledValues(cancelTime: number): AudioParam {
		checkTime('cancelScheduledValues', 'cancelTime', cancelTime);
		const i = _(this);
		$.audioParamCancel(i.handle, i.index, cancelTime);
		return this;
	}

	cancelAndHoldAtTime(cancelTime: number): AudioParam {
		throw new Error('Method not implemented.');
	}
}
def(AudioParam);
