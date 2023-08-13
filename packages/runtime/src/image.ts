/**
 * https://developer.mozilla.org/en-US/docs/Web/API/HTMLImageElement
 */
import { def } from './utils';
import { INTERNAL_SYMBOL } from './types';
import { toPromise, type Switch as _Switch } from './switch';
import type { ImageOpaque } from './switch';

declare const Switch: _Switch;

export class Image extends EventTarget {
	onload: ((this: Image, ev: Event) => any) | null;
	onerror: OnErrorEventHandler;
	decoding: globalThis.HTMLImageElement['decoding'];
	isMap: boolean;
	loading: globalThis.HTMLImageElement['loading'];

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
