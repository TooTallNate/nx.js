import { def } from '../utils';

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
	#map = new Map<string, string[]>();

	constructor(init?: HeadersInit) {
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
		const map = this.#map;
		let values = map.get(name);
		if (!values) {
			values = [];
			map.set(name, values);
		}
		values.push(value);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/delete) */
	delete(name: string): void {
		const map = this.#map;
		map.delete(normalizeName(name));
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/get) */
	get(name: string): string | null {
		name = normalizeName(name);
		const map = this.#map;
		const values = map.get(name);
		return values ? getValues(values) : null;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/getSetCookie) */
	getSetCookie(): string[] {
		const map = this.#map;
		return [...(map.get('set-cookie') || [])];
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/has) */
	has(name: string): boolean {
		const map = this.#map;
		return map.has(normalizeName(name));
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Headers/set) */
	set(name: string, value: string): void {
		const map = this.#map;
		map.set(normalizeName(name), [normalizeValue(value)]);
	}

	forEach(
		callbackfn: (value: string, key: string, parent: Headers) => void,
		thisArg?: any,
	): void {
		for (const [name, value] of this.entries()) {
			callbackfn.call(thisArg, value, name, this);
		}
	}

	/** Returns an iterator allowing to go through all key/value pairs contained in this object. */
	*entries(): HeadersIterator<[string, string]> {
		const map = this.#map;
		const sorted = [...map.entries()].sort((a, b) =>
			a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0,
		);
		for (const [name, values] of sorted) {
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
	*keys(): HeadersIterator<string> {
		for (const [name] of this.entries()) {
			yield name;
		}
	}

	/** Returns an iterator allowing to go through all values of the key/value pairs contained in this object. */
	*values(): HeadersIterator<string> {
		for (const [_n, value] of this.entries()) {
			yield value;
		}
	}

	/**
	 * Same as {@link Headers.entries | `entries()`}.
	 */
	[Symbol.iterator](): HeadersIterator<[string, string]> {
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
	const s = typeof v === 'string' ? v : String(v);
	return s.replace(/^[\t ]+|[\t ]+$/g, '');
}

const getValues = (v: string[]) => v.join(', ');
