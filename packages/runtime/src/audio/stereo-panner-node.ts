import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { AudioNode, type AudioNodeOptions } from './audio-node';
import { createAudioParam } from './audio-param';
import { ctxInternal, NODE_TYPE_STEREO_PANNER } from './internal';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface StereoPannerOptions extends AudioNodeOptions {
	pan?: number;
}

const _ = createInternal<StereoPannerNode, { pan: AudioParam }>();

/**
 * An {@link AudioNode} which pans its input across the stereo field using
 * the equal-power panning algorithm.
 *
 * @see https://developer.mozilla.org/docs/Web/API/StereoPannerNode
 */
export class StereoPannerNode
	extends AudioNode
	implements globalThis.StereoPannerNode
{
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/StereoPannerNode/StereoPannerNode
	 */
	constructor(context: BaseAudioContext, options: StereoPannerOptions = {}) {
		const handle = $.audioNodeNew(
			ctxInternal(context).handle,
			NODE_TYPE_STEREO_PANNER,
		);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			context,
			handle,
			numberOfInputs: 1,
			numberOfOutputs: 1,
			channelCount: options.channelCount ?? 2,
			channelCountMode: options.channelCountMode ?? 'clamped-max',
			channelInterpretation: options.channelInterpretation ?? 'speakers',
		});
		const pan = createAudioParam(this, handle, 0, {
			defaultValue: 0,
			minValue: -1,
			maxValue: 1,
		});
		if (typeof options.pan === 'number') {
			pan.value = options.pan;
		}
		_.set(this, { pan });
	}

	/**
	 * The position of the input in the stereo field (an a-rate
	 * {@link AudioParam}): `-1` = full left, `0` = center, `1` = full right.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/StereoPannerNode/pan
	 */
	get pan(): AudioParam {
		return _(this).pan;
	}
}
def(StereoPannerNode);
