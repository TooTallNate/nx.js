import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { createInternal, def } from '../utils';
import { AudioNode, type AudioNodeOptions } from './audio-node';
import { createAudioParam } from './audio-param';
import { ctxInternal, NODE_TYPE_GAIN } from './internal';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface GainOptions extends AudioNodeOptions {
	gain?: number;
}

const _ = createInternal<GainNode, { gain: AudioParam }>();

/**
 * An {@link AudioNode} which applies a (possibly automated) gain to its
 * input — e.g. volume control and fades.
 *
 * @see https://developer.mozilla.org/docs/Web/API/GainNode
 */
export class GainNode extends AudioNode implements globalThis.GainNode {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/GainNode/GainNode
	 */
	constructor(context: BaseAudioContext, options: GainOptions = {}) {
		const handle = $.audioNodeNew(ctxInternal(context).handle, NODE_TYPE_GAIN);
		// @ts-expect-error internal constructor
		super(INTERNAL_SYMBOL, {
			context,
			handle,
			numberOfInputs: 1,
			numberOfOutputs: 1,
			channelCount: options.channelCount ?? 2,
			channelCountMode: options.channelCountMode ?? 'max',
			channelInterpretation: options.channelInterpretation ?? 'speakers',
		});
		const gain = createAudioParam(this, handle, 0, { defaultValue: 1 });
		if (typeof options.gain === 'number') {
			gain.value = options.gain;
		}
		_.set(this, { gain });
	}

	/**
	 * The amount of gain to apply (an a-rate {@link AudioParam}).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/GainNode/gain
	 */
	get gain(): AudioParam {
		return _(this).gain;
	}
}
def(GainNode);
