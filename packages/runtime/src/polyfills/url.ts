import 'core-js/actual/url';
import 'core-js/actual/url-search-params';
import { Blob } from './blob';
import { crypto } from '../crypto';
import { inspect } from '../inspect';

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/URLSearchParams) */
export declare class URLSearchParams implements globalThis.URLSearchParams {
	constructor(
		init?: string[][] | Record<string, string> | string | URLSearchParams
	);
	size: number;
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
export declare class URL implements globalThis.URL {
	/**
	 * Constructs a new URL object by parsing the specified URL.
	 * @param url - The input URL to be parsed.
	 * @param base - The base URL to use in case the input URL is a relative URL.
	 */
	constructor(url: string | URL, base?: string | URL);

	hash: string; // The 'fragment' part of the URL including the '#'.
	host: string; // The host (that is the hostname, and the port if specified) of the URL.
	hostname: string; // The domain of the URL.
	href: string; // The entire URL.
	origin: string; // The origin of the URL.
	password: string; // The password specified before the domain name.
	pathname: string; // The path (that is the URL itself, excluding any parameters or #fragment) of the URL.
	port: string; // The port number of the URL.
	protocol: string; // The protocol of the URL.
	search: string; // The parameters of the URL.
	searchParams: URLSearchParams; // An object allowing to access the GET query arguments contained in the URL.
	username: string; // The username specified before the domain name.

	/**
	 * Returns a string containing the full URL. It is a synonym for the {@link URL.href} property, though it can't be used to modify the value.
	 */
	toString(): string;

	/**
	 * Returns a string containing the full URL. It is a synonym for the {@link URL.href} property.
	 */
	toJSON(): string;

	/**
	 * Returns a string containing a URL which represents the provided {@link Blob | `Blob`} object.
	 *
	 * @param obj - The object for which an object URL is to be created.
	 */
	static createObjectURL(obj: Blob): string;

	/**
	 * Revokes an object URL previously created using URL.createObjectURL().
	 *
	 * @param url - A string representing a URL that was created by calling URL.createObjectURL().
	 */
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

// @ts-expect-error `inspect.custom` is not defined on URL class
URL.prototype[inspect.custom] = function (this: URL) {
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
};
