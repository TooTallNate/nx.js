import type { IFont } from 'parse-css-font';
import { INTERNAL_SYMBOL } from './switch';
import type { Switch as _Switch, FontFaceState } from './switch';

declare const Switch: _Switch;

export class FontFaceSet extends EventTarget {
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

	_findFont(desired: IFont): FontFace | null {
		if (!desired.family) {
			throw new Error('No font-family was specified');
		}
		for (const family of desired.family) {
			for (const fontFace of this[INTERNAL_SYMBOL].fonts) {
				if (family === fontFace.family) {
					return fontFace;
				}
			}
		}
		return null;
	}

	_addSystemFont(): FontFace {
		const data = Switch.native.getSystemFont();
		const font = new FontFace('system-ui', data);
		this.add(font);
		return font;
	}
}

export class FontFace implements globalThis.FontFace {
	ascentOverride: string;
	descentOverride: string;
	display: FontDisplay;
	family: string;
	featureSettings: string;
	lineGapOverride: string;
	loaded: Promise<globalThis.FontFace>;
	status: FontFaceLoadStatus;
	stretch: string;
	style: string;
	unicodeRange: string;
	variant: string;
	weight: string;
	[INTERNAL_SYMBOL]: {
		data: ArrayBuffer;
		fontFace: FontFaceState;
	};

	constructor(
		family: string,
		source: string | BinaryData,
		descriptors?: FontFaceDescriptors
	) {
		if (typeof source === 'string') {
			throw new Error('Font `source` must be an ArrayBuffer');
		}
		this.family = family;
		this.ascentOverride = descriptors?.ascentOverride ?? '';
		this.descentOverride = descriptors?.descentOverride ?? '';
		this.display = descriptors?.display ?? 'auto';
		this.featureSettings = descriptors?.featureSettings ?? '';
		this.lineGapOverride = descriptors?.lineGapOverride ?? '';
		this.loaded = Promise.resolve(this);
		this.status = 'loaded';
		this.stretch = descriptors?.stretch ?? '';
		this.style = descriptors?.style ?? '';
		this.unicodeRange = descriptors?.unicodeRange ?? '';
		this.variant = descriptors?.variant ?? '';
		this.weight = descriptors?.weight ?? '';
		const buffer = source instanceof ArrayBuffer ? source : source.buffer;
		this[INTERNAL_SYMBOL] = {
			data: buffer,
			fontFace: Switch.native.newFontFace(buffer),
		};
	}

	load(): Promise<globalThis.FontFace> {
		throw new Error('Method not implemented.');
	}
}
