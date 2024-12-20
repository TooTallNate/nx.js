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

export interface AesCbcParams {
	name: 'AES-CBC';
	iv: BufferSource;
}
export interface AesCtrParams extends Algorithm {
	counter: BufferSource;
	length: number;
}
export interface AesXtsParams {
	name: 'AES-XTS';
	sectorSize: number;
	sector?: number;
	isNintendo?: boolean;
}
export interface AesDerivedKeyParams extends Algorithm {
	length: number;
}
export interface AesGcmParams extends Algorithm {
	additionalData?: BufferSource;
	iv: BufferSource;
	tagLength?: number;
}
export interface AesKeyAlgorithm extends KeyAlgorithm {
	length: number;
}
export interface AesKeyGenParams extends Algorithm {
	length: number;
}
export interface Algorithm {
	name: string;
}
export interface EcKeyAlgorithm extends KeyAlgorithm {
	namedCurve: NamedCurve;
}
export interface EcKeyGenParams extends Algorithm {
	namedCurve: NamedCurve;
}
export interface EcKeyImportParams extends Algorithm {
	namedCurve: NamedCurve;
}
export interface EcdhKeyDeriveParams extends Algorithm {
	public: CryptoKey;
}
export interface EcdsaParams extends Algorithm {
	hash: HashAlgorithmIdentifier;
}
export interface HkdfParams extends Algorithm {
	hash: HashAlgorithmIdentifier;
	info: BufferSource;
	salt: BufferSource;
}
export interface HmacImportParams extends Algorithm {
	hash: HashAlgorithmIdentifier;
	length?: number;
}
export interface HmacKeyAlgorithm extends KeyAlgorithm {
	hash: KeyAlgorithm;
	length: number;
}
export interface HmacKeyGenParams extends Algorithm {
	hash: HashAlgorithmIdentifier;
	length?: number;
}
export interface JsonWebKey {
	alg?: string;
	crv?: string;
	d?: string;
	dp?: string;
	dq?: string;
	e?: string;
	ext?: boolean;
	k?: string;
	key_ops?: string[];
	kty?: string;
	n?: string;
	oth?: RsaOtherPrimesInfo[];
	p?: string;
	q?: string;
	qi?: string;
	use?: string;
	x?: string;
	y?: string;
}
export interface KeyAlgorithm {
	name: string;
}
export type NamedCurve = string;
export type AlgorithmIdentifier = Algorithm | string;
export type KeyFormat = 'jwk' | 'pkcs8' | 'raw' | 'spki';
export type KeyType = 'private' | 'public' | 'secret';
export type KeyUsage =
	| 'decrypt'
	| 'deriveBits'
	| 'deriveKey'
	| 'encrypt'
	| 'sign'
	| 'unwrapKey'
	| 'verify'
	| 'wrapKey';
export interface RsaHashedImportParams extends Algorithm {
	hash: HashAlgorithmIdentifier;
}
export interface RsaHashedKeyAlgorithm extends RsaKeyAlgorithm {
	hash: KeyAlgorithm;
}
export interface RsaHashedKeyGenParams extends RsaKeyGenParams {
	hash: HashAlgorithmIdentifier;
}
export interface RsaKeyAlgorithm extends KeyAlgorithm {
	modulusLength: number;
	publicExponent: BigInteger;
}
export interface RsaKeyGenParams extends Algorithm {
	modulusLength: number;
	publicExponent: BigInteger;
}
export interface RsaOaepParams extends Algorithm {
	label?: BufferSource;
}
export interface RsaOtherPrimesInfo {
	d?: string;
	r?: string;
	t?: string;
}
export interface RsaPssParams extends Algorithm {
	saltLength: number;
}
