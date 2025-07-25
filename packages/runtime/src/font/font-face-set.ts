import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { EventTarget } from '../polyfills/event-target';
import { assertInternalConstructor, createInternal, def } from '../utils';
import { FontFace } from './font-face';
import type { screen } from '../screen';
import type { IFont } from 'parse-css-font';
import type { FontFaceSetLoadStatus } from '../types';

const _ = createInternal<FontFaceSet, Set<FontFace>>();

/**
 * Manages the loading of font-faces and querying of their download status.
 *
 * @see https://developer.mozilla.org/docs/Web/API/FontFaceSet
 */
export class FontFaceSet extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		_.set(this, new Set());
		this.ready = Promise.resolve(this);
		this.status = 'loaded';
	}

	onloading: ((this: FontFaceSet, ev: Event) => any) | null = null;
	onloadingdone: ((this: FontFaceSet, ev: Event) => any) | null = null;
	onloadingerror: ((this: FontFaceSet, ev: Event) => any) | null = null;
	ready: Promise<this>;
	status: FontFaceSetLoadStatus;
	check(font: string, text?: string | undefined): boolean {
		throw new Error('Method not implemented.');
	}
	load(font: string, text?: string | undefined): Promise<FontFace[]> {
		throw new Error('Method not implemented.');
	}

	// Set interface
	get size() {
		return _(this).size;
	}
	add(font: FontFace) {
		_(this).add(font);
		return this;
	}
	clear(): void {
		_(this).clear();
	}
	delete(font: FontFace): boolean {
		return _(this).delete(font);
	}
	has(font: FontFace): boolean {
		return _(this).has(font);
	}
	keys(): IterableIterator<FontFace> {
		return _(this).keys();
	}
	values(): IterableIterator<FontFace> {
		return _(this).values();
	}
	entries(): IterableIterator<[FontFace, FontFace]> {
		return _(this).entries();
	}
	forEach(
		callbackfn: (value: FontFace, key: FontFace, parent: FontFaceSet) => void,
		thisArg: any = this,
	): void {
		for (const font of _(this)) {
			callbackfn.call(thisArg, font, font, this);
		}
	}
	[Symbol.iterator](): IterableIterator<FontFace> {
		return _(this)[Symbol.iterator]();
	}
}
def(FontFaceSet);

/**
 * Contains the available fonts for use on the {@link screen | `screen`} Canvas context.
 *
 * There are two built-in fonts available:
 *
 *  - `"system-ui"` is the system font provided by the Switch operating system.
 *  - `"system-icons"` contains the icons used by the Switch operating system.
 *
 * Custom fonts can be added to the set using the {@link FontFaceSet.add | `add()`} method.
 *
 * @see https://nxjs.n8.io/runtime/concepts/fonts
 */
// @ts-expect-error Internal constructor
export var fonts = new FontFaceSet(INTERNAL_SYMBOL);
def(fonts, 'fonts');

export function findFont(
	fontFaceSet: FontFaceSet,
	desired: IFont,
): FontFace | null {
	if (!desired.family) {
		throw new Error('No `font-family` was specified');
	}
	for (const family of desired.family) {
		for (const fontFace of fontFaceSet) {
			if (
				family === fontFace.family &&
				desired.stretch === fontFace.stretch &&
				desired.style === fontFace.style &&
				desired.weight === fontFace.weight
			) {
				return fontFace;
			}
		}
	}
	return null;
}

export function addSystemFont(fonts: FontFaceSet): FontFace {
	const data = $.getSystemFont(0 /* PlSharedFontType_Standard */);
	const f = new FontFace('system-ui', data);
	fonts.add(f);
	fonts.add(new FontFace('sans-serif', data));
	return f;
}

export function addIconFont(fonts: FontFaceSet): FontFace {
	const data = $.getSystemFont(5 /* PlSharedFontType_NintendoExt */);
	const f = new FontFace('system-icons', data);
	fonts.add(f);
	return f;
}
