import { def } from '../utils';

const headerMaps = new WeakMap<Headers, Map<string, string>>();

export class Headers implements globalThis.Headers {
	constructor(init?: HeadersInit) {
		headerMaps.set(this, new Map());

		if (init instanceof globalThis.Headers) {
			for (const [name, value] of init) {
				this.append(name, value);
			}
		} else if (Array.isArray(init)) {
			for (const header of init) {
				if (header.length != 2) {
					throw new TypeError(
						`Headers constructor: expected name/value pair to be length 2, found: ${header.length}`
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

	append(name: string, value: string): void {
		name = normalizeName(name);
		value = normalizeValue(value);
		const map = headerMaps.get(this)!;
		const oldValue = map.get(name);
		map.set(name, oldValue ? `${oldValue}, ${value}` : value);
	}

	delete(name: string): void {
		const map = headerMaps.get(this)!;
		map.delete(normalizeName(name));
	}

	get(name: string): string | null {
		name = normalizeName(name);
		const map = headerMaps.get(this)!;
		return map.get(name) ?? null;
	}

	has(name: string): boolean {
		const map = headerMaps.get(this)!;
		return map.has(normalizeName(name));
	}

	set(name: string, value: string): void {
		const map = headerMaps.get(this)!;
		map.set(normalizeName(name), normalizeValue(value));
	}

	forEach(
		callbackfn: (
			value: string,
			key: string,
			parent: Headers
		) => void,
		thisArg?: any
	): void {
		const map = headerMaps.get(this)!;
		for (const [name, value] of map) {
			callbackfn.call(thisArg, value, name, this);
		}
	}

	entries(): IterableIterator<[string, string]> {
		const map = headerMaps.get(this)!;
		return map.entries();
	}

	keys(): IterableIterator<string> {
		const map = headerMaps.get(this)!;
		return map.keys();
	}

	values(): IterableIterator<string> {
		const map = headerMaps.get(this)!;
		return map.values();
	}

	[Symbol.iterator](): IterableIterator<[string, string]> {
		return this.entries();
	}
}

function normalizeName(v: unknown) {
	const name = typeof v === 'string' ? v : String(v);
	if (/[^a-z0-9\-#$%&'*+.^_`|~!]/i.test(name) || name === '') {
		throw new TypeError(
			`Invalid character in header field name: "${name}"`
		);
	}
	return name.toLowerCase();
}

function normalizeValue(v: unknown) {
	return typeof v === 'string' ? v : String(v);
}

def('Headers', Headers);
