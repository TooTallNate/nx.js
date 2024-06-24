import type { Image } from './image';
import type { Screen } from './screen';
import type { Blob } from './polyfills/blob';
import type { ImageData } from './canvas/image-data';
import type { ImageBitmap } from './canvas/image-bitmap';
import type { OffscreenCanvas } from './canvas/offscreen-canvas';

export type DOMHighResTimeStamp = number;

export interface ArrayBufferView {
	/**
	 * The ArrayBuffer instance referenced by the array.
	 */
	buffer: ArrayBuffer;

	/**
	 * The length in bytes of the array.
	 */
	byteLength: number;

	/**
	 * The offset in bytes of the array.
	 */
	byteOffset: number;
}

export type BufferSource = ArrayBufferView | ArrayBuffer;

export interface ImageEncodeOptions {
	quality?: number;
	type?: string;
}

export type CanvasFillRule = 'evenodd' | 'nonzero';
export type CanvasImageSource = Image | ImageBitmap | Screen | OffscreenCanvas;
export type CanvasLineCap = 'butt' | 'round' | 'square';
export type CanvasLineJoin = 'bevel' | 'miter' | 'round';
export type CanvasTextAlign = 'center' | 'end' | 'left' | 'right' | 'start';
export type CanvasTextBaseline =
	| 'alphabetic'
	| 'bottom'
	| 'hanging'
	| 'ideographic'
	| 'middle'
	| 'top';
export type GlobalCompositeOperation =
	| 'color'
	| 'color-burn'
	| 'color-dodge'
	| 'copy'
	| 'darken'
	| 'destination-atop'
	| 'destination-in'
	| 'destination-out'
	| 'destination-over'
	| 'difference'
	| 'exclusion'
	| 'hard-light'
	| 'hue'
	| 'lighten'
	| 'lighter'
	| 'luminosity'
	| 'multiply'
	| 'overlay'
	| 'saturate'
	| 'saturation'
	| 'screen'
	| 'soft-light'
	| 'source-atop'
	| 'source-in'
	| 'source-out'
	| 'source-over'
	| 'xor';
export type ImageSmoothingQuality = 'high' | 'low' | 'medium';
export interface TextMetrics {
	// x-direction
	width: number; // advance width
	actualBoundingBoxLeft: number;
	actualBoundingBoxRight: number;

	// y-direction
	fontBoundingBoxAscent: number;
	fontBoundingBoxDescent: number;
	actualBoundingBoxAscent: number;
	actualBoundingBoxDescent: number;
	emHeightAscent: number;
	emHeightDescent: number;
	hangingBaseline: number;
	alphabeticBaseline: number;
	ideographicBaseline: number;
}

export type FontDisplay = 'auto' | 'block' | 'fallback' | 'optional' | 'swap';
export type FontFaceLoadStatus = 'error' | 'loaded' | 'loading' | 'unloaded';
export type FontFaceSetLoadStatus = 'loaded' | 'loading';
export interface FontFaceDescriptors {
	ascentOverride?: string;
	descentOverride?: string;
	display?: FontDisplay;
	featureSettings?: string;
	lineGapOverride?: string;
	stretch?: string;
	style?: string;
	unicodeRange?: string;
	weight?: string;
}

export type ColorSpaceConversion = 'default' | 'none';
export type ImageOrientation = 'flipY' | 'from-image' | 'none';
export type PremultiplyAlpha = 'default' | 'none' | 'premultiply';
export type ResizeQuality = 'high' | 'low' | 'medium' | 'pixelated';
export type ImageBitmapSource = CanvasImageSource | Blob | ImageData;

export type GamepadMappingType = '' | 'standard' | 'xr-standard';
export type GamepadHapticActuatorType = 'vibration';
export interface GamepadEffectParameters {
	duration?: number;
	startDelay?: number;
	strongMagnitude?: number;
	weakMagnitude?: number;
}
