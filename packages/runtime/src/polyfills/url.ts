import { $ } from '../$';
import { Blob } from './blob';
import { def, proto, stub } from '../utils';
import { crypto } from '../crypto';
import { inspect } from '../switch/inspect';

export const objectUrls = new Map<string, Blob>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams) */
export class URLSearchParams implements globalThis.URLSearchParams {
	constructor(
		init?: string[][] | Record<string, string> | string | URLSearchParams,
	) {
		let input = '';
		if (init && (typeof init === 'string' || init instanceof URLSearchParams)) {
			input = String(init);
		}
		const p = proto($.urlSearchNew(input, arguments[1]), URLSearchParams);
		if (!input) {
			if (Array.isArray(init)) {
				for (const [k, v] of init) {
					p.append(k, v);
				}
			} else if (init && typeof init === 'object') {
				for (const [k, v] of Object.entries(init)) {
					p.set(k, v);
				}
			}
		}
		return p;
	}

	/**
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/size)
	 */
	declare readonly size: number;

	/**
	 * Appends a specified key/value pair as a new search parameter.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/append)
	 */
	append(name: string, value: string): void {
		stub();
	}

	/**
	 * Deletes the given search parameter, and its associated value, from the list of all search parameters.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/delete)
	 */
	delete(name: string, value?: string): void {
		stub();
	}

	/**
	 * Returns the first value associated to the given search parameter.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/get)
	 */
	get(name: string): string | null {
		stub();
	}

	/**
	 * Returns all the values association with a given search parameter.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/getAll)
	 */
	getAll(name: string): string[] {
		stub();
	}

	/**
	 * Returns a Boolean indicating if such a search parameter exists.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/has)
	 */
	has(name: string): boolean {
		stub();
	}

	/**
	 * Sets the value associated to a given search parameter to the given value. If there were several values, delete the others.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/URLSearchParams/set
	 */
	set(name: string, value: string): void {
		stub();
	}

	/**
	 * Sorts all key/value pairs contained in this object in place and returns
	 * `undefined`. The sort order is according to unicode code points of the
	 * keys. This method uses a stable sorting algorithm (i.e. the relative
	 * order between key/value pairs with equal keys will be preserved).
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/URLSearchParams/sort
	 */
	sort(): void {
		stub();
	}

	/**
	 * Returns a string containing a query string suitable for use in a URL. Does not include the question mark.
	 *
	 * @example
	 *
	 * ```typescript
	 * const params = new URLSearchParams({ foo: '1', bar: '2' });
	 *
	 * // Add a second foo parameter.
	 * params.append("foo", 4);
	 *
	 * params.toString();
	 * // Returns "foo=1&bar=2&foo=4"
	 * ```
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/URLSearchParams/toString
	 */
	toString(): string {
		stub();
	}

	forEach(
		callbackfn: (value: string, key: string, parent: URLSearchParams) => void,
		thisArg: any = this,
	): void {
		for (const [k, v] of this) {
			callbackfn.call(thisArg, v, k, this);
		}
	}

	/** Returns a list of keys in the search params. */
	*keys(): IterableIterator<string> {
		const it = $.urlSearchIterator(this, 0);
		while (1) {
			const n = $.urlSearchIteratorNext(it);
			if (typeof n !== 'string') break;
			yield n;
		}
	}

	/** Returns a list of values in the search params. */
	*values(): IterableIterator<string> {
		const it = $.urlSearchIterator(this, 1);
		while (1) {
			const n = $.urlSearchIteratorNext(it);
			if (typeof n !== 'string') break;
			yield n;
		}
	}

	/** Returns an array of key, value pairs for every entry in the search params. */
	*entries(): IterableIterator<[string, string]> {
		const it = $.urlSearchIterator(this, 2);
		while (1) {
			const n = $.urlSearchIteratorNext(it);
			if (!n) break;
			yield n;
		}
	}

	[Symbol.iterator](): IterableIterator<[string, string]> {
		return this.entries();
	}
}
$.urlSearchInit(URLSearchParams);
def(URLSearchParams);

Object.defineProperty(URLSearchParams.prototype, inspect.entries, {
	enumerable: false,
	value(this: URLSearchParams) {
		return this.entries();
	},
});

const searchParams = new WeakMap<URL, URLSearchParams>();

/**
 * The `URL` interface is used to parse, construct, normalize, and encode URLs.
 * It works by providing properties which allow you to easily read and modify
 * the components of a URL.
 *
 * You normally create a new `URL` object by specifying the URL as a string
 * when calling its constructor, or by providing a relative URL and a base
 * URL. You can then easily read the parsed components of the URL or make
 * changes to the URL.
 */
export class URL implements globalThis.URL {
	/**
	 * Constructs a new URL object by parsing the specified URL.
	 *
	 * @param url The input URL to be parsed.
	 * @param base The base URL to use in case the input URL is a relative URL.
	 */
	constructor(url: string | URL, base?: string | URL) {
		return proto($.urlNew(url, base), URL);
	}

	declare hash: string;
	declare host: string;
	declare hostname: string;
	declare href: string;
	declare password: string;
	declare pathname: string;
	declare port: string;
	declare protocol: string;
	declare search: string;
	declare username: string;
	declare readonly origin: string;

	get searchParams(): URLSearchParams {
		let p = searchParams.get(this);
		if (!p) {
			p = new URLSearchParams(
				this.search,
				// @ts-expect-error internal argument
				this,
			);
			searchParams.set(this, p);
		}
		return p;
	}

	/**
	 * Returns a string containing the full URL. It is a synonym for the {@link URL.href | `href`} property, though it can't be used to modify the value.
	 */
	toString(): string {
		return this.href;
	}

	/**
	 * Returns a string containing the full URL. It is a synonym for the {@link URL.href | `href`} property.
	 */
	toJSON(): string {
		return this.href;
	}

	/**
	 * Returns a string containing a URL which represents the provided {@link Blob | `Blob`} object.
	 *
	 * @param obj The object for which an object URL is to be created.
	 * @see https://developer.mozilla.org/docs/Web/API/URL/createObjectURL_static
	 */
	static createObjectURL(obj: Blob): string {
		if (!(obj instanceof Blob)) {
			throw new Error('Must be Blob');
		}
		const url = `blob:${crypto.randomUUID()}`;
		objectUrls.set(url, obj);
		return url;
	}

	/**
	 * Revokes an object URL previously created using {@link URL.createObjectURL | `URL.createObjectURL()`}.
	 *
	 * @param url A string representing a URL that was created by calling `URL.createObjectURL()`.
	 * @see https://developer.mozilla.org/docs/Web/API/URL/revokeObjectURL_static
	 */
	static revokeObjectURL(url: string): void {
		objectUrls.delete(url);
	}

	/**
	 * Returns a boolean indicating whether or not an absolute URL, or
	 * a relative URL combined with a base URL, are parsable and valid.
	 *
	 * @param url The input URL to be parsed.
	 * @param base The base URL to use in case the input URL is a relative URL.
	 * @see https://developer.mozilla.org/docs/Web/API/URL/canParse_static
	 */
	static canParse(url: string | URL, base?: string | URL) {
		stub();
	}
}
$.urlInit(URL);
def(URL);

Object.defineProperty(URL.prototype, inspect.keys, {
	enumerable: false,
	value: () => [
		'hash',
		'host',
		'hostname',
		'href',
		'origin',
		'password',
		'pathname',
		'port',
		'protocol',
		'search',
		'searchParams',
		'username',
	],
});
