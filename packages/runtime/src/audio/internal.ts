// Shared internal state for the Web Audio classes. Lives in its own module so
// that sibling modules (audio-node.ts, base-audio-context.ts, the node
// subclasses) can access each other's internals without import cycles — all
// cross-module imports of this file are value imports, while the class types
// are imported `type`-only.
import type { AudioContextHandle, AudioNodeHandle } from '../$';
import { createInternal } from '../utils';
import type { AudioBuffer } from './audio-buffer';
import type { AudioDestinationNode } from './audio-destination-node';
import type { AudioNode } from './audio-node';
import type { AudioParam } from './audio-param';
import type { BaseAudioContext } from './base-audio-context';

export interface BaseAudioContextInternal {
	handle: AudioContextHandle;
	state: AudioContextState;
	sampleRate: number;
	destination?: AudioDestinationNode;
}

export const ctxInternal = createInternal<
	BaseAudioContext,
	BaseAudioContextInternal
>();

export interface AudioNodeInternal {
	context: BaseAudioContext;
	handle: AudioNodeHandle;
	numberOfInputs: number;
	numberOfOutputs: number;
	channelCount: number;
	channelCountMode: ChannelCountMode;
	channelInterpretation: ChannelInterpretation;
}

export const nodeInternal = createInternal<AudioNode, AudioNodeInternal>();

export interface AudioParamInternal {
	/**
	 * The owning node — retained so that a reachable `AudioParam` keeps the
	 * native node (and therefore the param storage) alive.
	 */
	node: AudioNode;
	handle: AudioNodeHandle;
	index: number;
	defaultValue: number;
	minValue: number;
	maxValue: number;
	automationRate: AutomationRate;
}

export const paramInternal = createInternal<AudioParam, AudioParamInternal>();

export interface AudioBufferInternal {
	channels: Float32Array[];
	sampleRate: number;
	length: number;
}

export const bufferInternal = createInternal<AudioBuffer, AudioBufferInternal>();

// Native node type discriminators — must match `nx_audio_node_type` in
// `source/audio-graph.h`.
export const NODE_TYPE_GAIN = 1;
export const NODE_TYPE_STEREO_PANNER = 2;
export const NODE_TYPE_BUFFER_SOURCE = 3;

// AudioParam automation event types — must match `nx_audio_param_event_type`
// in `source/audio-graph.h`.
export const PARAM_EVENT_SET_VALUE = 0;
export const PARAM_EVENT_LINEAR_RAMP = 1;
export const PARAM_EVENT_EXPONENTIAL_RAMP = 2;
export const PARAM_EVENT_SET_TARGET = 3;

// `nx_audio_source_state` values from `source/audio-graph.h`.
export const SOURCE_STATE_FINISHED = 2;

/** Maximum value of a single-precision float (AudioParam default min/max). */
export const MOST_POSITIVE_SINGLE = 3.4028234663852886e38;
