import 'core-js/actual/url';
import 'core-js/actual/url-search-params';
import { Blob } from './blob';
import { crypto } from '../crypto';

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams) */
export declare class URLSearchParams implements globalThis.URLSearchParams {
	constructor(
		init?: string[][] | Record<string, string> | string | URLSearchParams
	);
	/**
	 * Appends a specified key/value pair as a new search parameter.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/append)
	 */
	append(name: string, value: string): void;
	/**
	 * Deletes the given search parameter, and its associated value, from the list of all search parameters.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/delete)
	 */
	delete(name: string): void;
	/**
	 * Returns the first value associated to the given search parameter.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/get)
	 */
	get(name: string): string | null;
	/**
	 * Returns all the values association with a given search parameter.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/getAll)
	 */
	getAll(name: string): string[];
	/**
	 * Returns a Boolean indicating if such a search parameter exists.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/has)
	 */
	has(name: string): boolean;
	/**
	 * Sets the value associated to a given search parameter to the given value. If there were several values, delete the others.
	 *
	 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/set)
	 */
	set(name: string, value: string): void;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams/sort) */
	sort(): void;

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
	toString(): string;

	forEach(
		callbackfn: (
			value: string,
			key: string,
			parent: URLSearchParams
		) => void,
		thisArg?: any
	): void;

	/** Returns an array of key, value pairs for every entry in the search params. */
	entries(): IterableIterator<[string, string]>;

	/** Returns a list of keys in the search params. */
	keys(): IterableIterator<string>;

	/** Returns a list of values in the search params. */
	values(): IterableIterator<string>;

	[Symbol.iterator](): IterableIterator<[string, string]>;
}

export declare class URL implements globalThis.URL {
	constructor(url: string | URL, base?: string | URL);
	hash: string;
	host: string;
	hostname: string;
	href: string;
	toString(): string;
	origin: string;
	password: string;
	pathname: string;
	port: string;
	protocol: string;
	search: string;
	searchParams: URLSearchParams;
	username: string;
	toJSON(): string;

	static createObjectURL(obj: Blob): void;
	static revokeObjectURL(url: string): void;
}

export const objectUrls = new Map<string, Blob>();

URL.createObjectURL = (obj) => {
	if (!(obj instanceof Blob)) {
		throw new Error('Must be Blob');
	}
	const url = `blob:${crypto.randomUUID()}`;
	objectUrls.set(url, obj);
	return url;
};

URL.revokeObjectURL = (url) => {
	objectUrls.delete(url);
};
