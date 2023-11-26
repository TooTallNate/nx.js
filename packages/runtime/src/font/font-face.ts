import { $ } from '../$';
import { bufferSourceToArrayBuffer, def } from '../utils';
import type { FontFaceLoadStatus, FontDisplay } from '../types';

/**
 * Defines the source of a font face, either a URL to an external resource or a
 * buffer, and font properties such as `style`, `weight`, and so on. For URL
 * font sources it allows authors to trigger when the remote font is fetched
 * and loaded, and to track loading status.
 *
 * @see https://developer.mozilla.org/docs/Web/API/FontFace
 */
export class FontFace implements globalThis.FontFace {
	declare ascentOverride: string;
	declare descentOverride: string;
	declare display: FontDisplay;
	declare family: string;
	declare featureSettings: string;
	declare lineGapOverride: string;
	declare readonly loaded: Promise<this>;
	declare readonly status: FontFaceLoadStatus;
	declare stretch: string;
	declare style: string;
	declare unicodeRange: string;
	declare variant: string;
	declare weight: string;

	constructor(
		family: string,
		source: string | BufferSource,
		descriptors: FontFaceDescriptors = {}
	) {
		if (typeof source === 'string') {
			throw new Error('Font `source` must be an ArrayBuffer');
		}
		const buffer = bufferSourceToArrayBuffer(source);
		const f = $.fontFaceNew(buffer);
		Object.setPrototypeOf(f, FontFace.prototype);
		f.family = family;
		f.ascentOverride = descriptors.ascentOverride ?? 'normal';
		f.descentOverride = descriptors.descentOverride ?? 'normal';
		f.display = descriptors.display ?? 'auto';
		f.featureSettings = descriptors.featureSettings ?? 'normal';
		f.lineGapOverride = descriptors.lineGapOverride ?? 'normal';
		// @ts-expect-error Readonly
		f.loaded = Promise.resolve(this);
		// @ts-expect-error Readonly
		f.status = 'loaded';
		f.stretch = descriptors.stretch ?? 'normal';
		f.style = descriptors.style ?? 'normal';
		f.unicodeRange = descriptors.unicodeRange ?? '';
		f.variant = descriptors.variant ?? 'normal';
		f.weight = descriptors.weight ?? 'normal';
		return f;
	}

	async load(): Promise<this> {
		throw new Error('Method not implemented.');
	}
}
def('FontFace', FontFace);
