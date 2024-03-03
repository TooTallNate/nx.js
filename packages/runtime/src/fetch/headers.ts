import { createInternal, def } from '../utils';

const _ = createInternal<Headers, Map<string, string[]>>();

export type HeadersInit = [string, string][] | Record<string, string> | Headers;

/**
 * This Fetch API interface allows you to perform various actions on HTTP request
 * and response headers. These actions include retrieving, setting, adding to,
 * and removing.
 *
 * A Headers object has an associated header list, which is initially
 * empty and consists of zero or more name and value pairs. You can add to this
 * using methods like {@link Headers.append | `append()`}. In all methods of this interface, header names
 * are matched by case-insensitive byte sequence.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers)
 */
export class Headers implements globalThis.Headers {
	constructor(init?: HeadersInit) {
		_.set(this, new Map());

		if (init instanceof Headers) {
			for (const [name, value] of init) {
				this.append(name, value);
			}
		} else if (Array.isArray(init)) {
			for (const header of init) {
				if (header.length != 2) {
					throw new TypeError(
						`Headers constructor: expected name/value pair to be length 2, found: ${header.length}`,
					);
				}
				this.append(header[0], header[1]);
			}
		} else if (init) {
			for (const name of Object.getOwnPropertyNames(init)) {
				this.append(name, init[name]);
			}
		}
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/append) */
	append(name: string, value: string): void {
		name = normalizeName(name);
		value = normalizeValue(value);
		const map = _(this);
		let values = map.get(name);
		if (!values) {
			values = [];
			map.set(name, values);
		}
		values.push(value);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/delete) */
	delete(name: string): void {
		const map = _(this);
		map.delete(normalizeName(name));
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/get) */
	get(name: string): string | null {
		name = normalizeName(name);
		const map = _(this);
		const values = map.get(name);
		return values ? getValues(values) : null;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/getSetCookie) */
	getSetCookie(): string[] {
		const map = _(this);
		return map.get('set-cookie') || [];
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/has) */
	has(name: string): boolean {
		const map = _(this);
		return map.has(normalizeName(name));
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/set) */
	set(name: string, value: string): void {
		const map = _(this);
		map.set(normalizeName(name), [value]);
	}

	forEach(
		callbackfn: (value: string, key: string, parent: Headers) => void,
		thisArg?: any,
	): void {
		const map = _(this);
		for (const [name, values] of map) {
			callbackfn.call(thisArg, getValues(values), name, this);
		}
	}

	/** Returns an iterator allowing to go through all key/value pairs contained in this object. */
	*entries(): IterableIterator<[string, string]> {
		const map = _(this);
		for (const [name, values] of map.entries()) {
			if (name === 'set-cookie') {
				for (const value of values) {
					yield [name, value];
				}
			} else {
				yield [name, getValues(values)];
			}
		}
	}

	/** Returns an iterator allowing to go through all keys of the key/value pairs contained in this object. */
	*keys(): IterableIterator<string> {
		for (const [name] of this.entries()) {
			yield name;
		}
	}

	/** Returns an iterator allowing to go through all values of the key/value pairs contained in this object. */
	*values(): IterableIterator<string> {
		for (const [_n, value] of this.entries()) {
			yield value;
		}
	}

	/**
	 * Same as {@link Headers.entries | `entries()`}.
	 */
	[Symbol.iterator](): IterableIterator<[string, string]> {
		return this.entries();
	}
}
def(Headers);

function normalizeName(v: unknown) {
	const name = typeof v === 'string' ? v : String(v);
	if (/[^a-z0-9\-#$%&'*+.^_`|~!]/i.test(name) || name === '') {
		throw new TypeError(`Invalid character in header field name: "${name}"`);
	}
	return name.toLowerCase();
}

function normalizeValue(v: unknown) {
	return typeof v === 'string' ? v : String(v);
}

const getValues = (v: string[]) => v.join(', ');
