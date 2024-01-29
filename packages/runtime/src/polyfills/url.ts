import { $ } from '../$';
import { Blob } from './blob';
import { def, stub } from '../utils';
import { crypto } from '../crypto';
import { inspect } from '../inspect';

export const objectUrls = new Map<string, Blob>();

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams) */
export class URLSearchParams implements globalThis.URLSearchParams {
	constructor(
		init?: string[][] | Record<string, string> | string | URLSearchParams
	) {
		let input = '';
		if (
			init &&
			(typeof init === 'string' || init instanceof URLSearchParams)
		) {
			input = String(init);
		}
		const p = $.urlSearchNew(input);
		Object.setPrototypeOf(p, URLSearchParams.prototype);
		if (Array.isArray(init)) {
			for (const [k, v] of init) {
				p.append(k, v);
			}
		} else if (init && typeof init === 'object') {
			for (const [k, v] of Object.entries(init)) {
				p.set(k, v);
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
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/set)
	 */
	set(name: string, value: string): void {
		stub();
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/sort) */
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
	 */
	toString(): string {
		stub();
	}

	forEach(
		callbackfn: (
			value: string,
			key: string,
			parent: URLSearchParams
		) => void,
		thisArg: any = this
	): void {
		for (const [k, v] of this) {
			callbackfn.call(thisArg, v, k, this);
		}
	}

	/** Returns an array of key, value pairs for every entry in the search params. */
	*entries(): IterableIterator<[string, string]> {}

	/** Returns a list of keys in the search params. */
	*keys(): IterableIterator<string> {}

	/** Returns a list of values in the search params. */
	*values(): IterableIterator<string> {}

	[Symbol.iterator](): IterableIterator<[string, string]> {
		return this.entries();
	}
}
$.urlSearchInit(URLSearchParams);
def(URLSearchParams);

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
		const u = $.urlNew(url, base);
		Object.setPrototypeOf(u, URL.prototype);
		return u;
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
	declare searchParams: URLSearchParams;
	declare username: string;
	declare readonly origin: string;

	/**
	 * Returns a string containing the full URL. It is a synonym for the {@link URL.href} property, though it can't be used to modify the value.
	 */
	toString(): string {
		return this.href;
	}

	/**
	 * Returns a string containing the full URL. It is a synonym for the {@link URL.href} property.
	 */
	toJSON(): string {
		return this.href;
	}

	/**
	 * Returns a string containing a URL which represents the provided {@link Blob | `Blob`} object.
	 *
	 * @param obj - The object for which an object URL is to be created.
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
	 * Revokes an object URL previously created using URL.createObjectURL().
	 *
	 * @param url - A string representing a URL that was created by calling URL.createObjectURL().
	 */
	static revokeObjectURL(url: string): void {
		objectUrls.delete(url);
	}
}
$.urlInit(URL);
def(URL);

Object.defineProperty(URL.prototype, inspect.custom, {
	enumerable: false,
	value(this: URL) {
		return `URL ${inspect({
			hash: this.hash,
			host: this.host,
			hostname: this.hostname,
			href: this.href,
			origin: this.origin,
			password: this.password,
			pathname: this.pathname,
			port: this.port,
			protocol: this.protocol,
			search: this.search,
			searchParams: this.searchParams,
			username: this.username,
		})}`;
	},
});
