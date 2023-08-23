import { def } from './utils';
import { INTERNAL_SYMBOL } from './types';
import { toPromise, type SwitchClass } from './switch';
import type { ImageOpaque } from './switch';

declare const Switch: SwitchClass;

/**
 * The `Image` class is the spiritual equivalent of the [`HTMLImageElement`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLImageElement)
 * class in web browsers. You can use it to load image data from the filesytem
 * or remote source over the network. Once loaded, the image may be drawn onto the screen
 * context or an offscreen canvas content using {@link CanvasRenderingContext2D.drawImage | `ctx.drawImage()`}.
 *
 * ### Currently Supported Image Formats
 *
 *  - `jpg` - JPEG image data using [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo)
 *  - `png` - PNG image data using [libpng](http://www.libpng.org/pub/png/libpng.html)
 *  - `webp` - WebP image data using [libpng](https://github.com/webmproject/libwebp)
 *
 * @example
 *
 * ```typescript
 * const ctx = Switch.screen.getContext('2d');
 *
 * const img = new Image();
 * img.addEventListener('load', () => {
 *   ctx.drawImage(img);
 * });
 * img.src = 'romfs:/logo.png';
 * ```
 */
export class Image extends EventTarget {
	onload: ((this: Image, ev: Event) => any) | null;
	onerror: OnErrorEventHandler;
	decoding: "async" | "sync" | "auto";
	isMap: boolean;
	loading: "eager" | "lazy";

	/**
	 * @ignore
	 */
	[INTERNAL_SYMBOL]: {
		complete: boolean;
		width: number;
		height: number;
		opaque?: ImageOpaque;
		src?: URL;
	};

	constructor() {
		super();
		this.onload = null;
		this.onerror = null;
		this.decoding = 'auto';
		this.isMap = false;
		this.loading = 'eager';
		this[INTERNAL_SYMBOL] = {
			complete: false,
			width: 0,
			height: 0,
		};
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'load') {
			if (typeof this.onload === 'function') {
				this.onload(event);
			}
		} else if (event.type === 'error') {
			if (typeof this.onerror === 'function') {
				this.onerror(event);
			}
		}
		return super.dispatchEvent(event);
	}

	get complete() {
		return this[INTERNAL_SYMBOL].complete;
	}

	get width() {
		return this[INTERNAL_SYMBOL].width;
	}

	get height() {
		return this[INTERNAL_SYMBOL].height;
	}

	get naturalWidth() {
		return this[INTERNAL_SYMBOL].width;
	}

	get naturalHeight() {
		return this[INTERNAL_SYMBOL].height;
	}

	get src() {
		return this[INTERNAL_SYMBOL].src?.href ?? '';
	}

	set src(val: string) {
		const url = new URL(val, Switch.entrypoint);
		const internal = this[INTERNAL_SYMBOL];
		internal.src = url;
		fetch(url)
			.then((res) => {
				if (!res.ok) {
					throw new Error(`Failed to load image: ${res.status}`);
				}
				return res.arrayBuffer();
			})
			.then((buf) => {
				return toPromise(Switch.native.decodeImage, buf);
			})
			.then(
				(r) => {
					internal.opaque = r.opaque;
					internal.width = r.width;
					internal.height = r.height;
					internal.complete = true;
					this.dispatchEvent(new Event('load'));
				},
				(error) => {
					internal.complete = true;
					this.dispatchEvent(new ErrorEvent('error', { error }));
				}
			);
	}
}
def('Image', Image);
