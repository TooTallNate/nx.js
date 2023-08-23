import type { IFont } from 'parse-css-font';
import { INTERNAL_SYMBOL } from '../types';
import type { SwitchClass, FontFaceState } from '../switch';
import { def } from '../utils';

declare const Switch: SwitchClass;

/**
 * Manages the loading of font-faces and querying of their download status.
 *
 * This property is available as {@link Switch.fonts | `Switch.fonts`}.
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/API/FontFaceSet
 */
export class FontFaceSet extends EventTarget {
	/**
	 * @ignore
	 */
	[INTERNAL_SYMBOL]: {
		fonts: Set<FontFace>;
	};

	constructor() {
		super();
		this[INTERNAL_SYMBOL] = {
			fonts: new Set(),
		};
	}

	add(font: FontFace) {
		this[INTERNAL_SYMBOL].fonts.add(font);
	}

	values(): IterableIterator<FontFace> {
		return this[INTERNAL_SYMBOL].fonts.values();
	}

}

export function findFont(
	fontFaceSet: FontFaceSet,
	desired: IFont
): FontFace | null {
	if (!desired.family) {
		throw new Error('No font-family was specified');
	}
	for (const family of desired.family) {
		for (const fontFace of fontFaceSet[INTERNAL_SYMBOL].fonts) {
			if (
				family === fontFace.family &&
				desired.stretch === fontFace.stretch &&
				desired.style === fontFace.style &&
				desired.variant === fontFace.variant &&
				desired.weight === fontFace.weight
			) {
				return fontFace;
			}
		}
	}
	return null;
}

export function addSystemFont(fontFaceSet: FontFaceSet): FontFace {
	const data = Switch.native.getSystemFont();
	const font = new FontFace('system-ui', data);
	fontFaceSet.add(font);
	return font;
}

export const fontFaceInternal = new WeakMap<
	FontFace,
	{
		data: ArrayBuffer;
		fontFace: FontFaceState;
	}
>();

export class FontFace implements globalThis.FontFace {
	ascentOverride: string;
	descentOverride: string;
	display: FontDisplay;
	family: string;
	featureSettings: string;
	lineGapOverride: string;
	loaded: Promise<FontFace>;
	status: FontFaceLoadStatus;
	stretch: string;
	style: string;
	unicodeRange: string;
	variant: string;
	weight: string;

	constructor(
		family: string,
		source: string | BinaryData,
		descriptors: FontFaceDescriptors = {}
	) {
		if (typeof source === 'string') {
			throw new Error('Font `source` must be an ArrayBuffer');
		}
		this.family = family;
		this.ascentOverride = descriptors.ascentOverride ?? 'normal';
		this.descentOverride = descriptors.descentOverride ?? 'normal';
		this.display = descriptors.display ?? 'auto';
		this.featureSettings = descriptors.featureSettings ?? 'normal';
		this.lineGapOverride = descriptors.lineGapOverride ?? 'normal';
		this.loaded = Promise.resolve(this);
		this.status = 'loaded';
		this.stretch = descriptors.stretch ?? 'normal';
		this.style = descriptors.style ?? 'normal';
		this.unicodeRange = descriptors.unicodeRange ?? '';
		this.variant = descriptors.variant ?? 'normal';
		this.weight = descriptors.weight ?? 'normal';
		const buffer = source instanceof ArrayBuffer ? source : source.buffer;
		fontFaceInternal.set(this, {
			data: buffer,
			fontFace: Switch.native.newFontFace(buffer),
		});
	}

	async load(): Promise<this> {
		throw new Error('Method not implemented.');
	}
}

def('FontFace', FontFace);
def('FontFaceSet', FontFaceSet);
