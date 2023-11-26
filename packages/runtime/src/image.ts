import { def, toPromise } from './utils';
import { INTERNAL_SYMBOL } from './internal';
import { type SwitchClass } from './switch';
import { fetch } from './fetch/fetch';
import { Event, ErrorEvent } from './polyfills/event';
import { EventTarget } from './polyfills/event-target';
import type { ImageOpaque } from './switch';
import type { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';

declare const Switch: SwitchClass;

/**
 * The `Image` class is the spiritual equivalent of the [`HTMLImageElement`](https://developer.mozilla.org/docs/Web/API/HTMLImageElement)
 * class in web browsers. You can use it to load image data from the filesytem
 * or remote source over the network. Once loaded, the image may be drawn onto the screen
 * context or an offscreen canvas context using {@link CanvasRenderingContext2D.drawImage | `ctx.drawImage()`}.
 *
 * ### Supported Image Formats
 *
 *  - `jpg` - JPEG image data using [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo)
 *  - `png` - PNG image data using [libpng](http://www.libpng.org/pub/png/libpng.html)
 *  - `webp` - WebP image data using [libpng](https://github.com/webmproject/libwebp)
 *
 * @example
 *
 * ```typescript
 * const ctx = screen.getContext('2d');
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
	onerror: ((this: Image, ev: ErrorEvent) => any) | null;
	decoding: 'async' | 'sync' | 'auto';
	isMap: boolean;
	loading: 'eager' | 'lazy';

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
			complete: true,
			width: 0,
			height: 0,
		};
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'load') {
			this.onload?.(event);
		} else if (event.type === 'error') {
			this.onerror?.(event as ErrorEvent);
		}
		return super.dispatchEvent(event);
	}

	get complete() {
		return this[INTERNAL_SYMBOL].complete;
	}

	get width() {
		return this.naturalWidth;
	}

	get height() {
		return this.naturalHeight;
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
		internal.complete = false;
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
					internal.complete = false;
					this.dispatchEvent(new ErrorEvent('error', { error }));
				}
			);
	}

	// Compat with HTML DOM interface
	className = '';
	get nodeType() {
		return 1;
	}
	get nodeName() {
		return 'IMG';
	}
	getAttribute(name: string): string | null {
		if (name === 'width') return String(this.width);
		if (name === 'height') return String(this.height);
		return null;
	}
	setAttribute(name: string, value: string | number) {}
}
def('Image', Image);
