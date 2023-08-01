import { def } from '../utils';

const dataWm = new WeakMap<FormData, [string, FormDataEntryValue][]>();

export class FormData implements globalThis.FormData {
	constructor() {
		dataWm.set(this, []);
	}

	append(name: string, value: string | Blob): void;
	append(name: string, value: string): void;
	append(name: string, blobValue: Blob, filename?: string): void;
	append(name: string, blobValue: string | Blob, filename?: string): void {
		const value =
			typeof blobValue === 'string'
				? blobValue
				: new File([blobValue], filename ?? name);
		const data = dataWm.get(this)!;
		data.push([name, value]);
	}

	delete(name: string): void {
		const data = dataWm.get(this)!;
		const updated = data.filter((entry) => entry[0] !== name);
		dataWm.set(this, updated);
	}

	get(name: string): FormDataEntryValue | null {
		const data = dataWm.get(this)!;
		for (const [key, value] of data) {
			if (key === name) return value;
		}
		return null;
	}

	getAll(name: string): FormDataEntryValue[] {
		const data = dataWm.get(this)!;
		const values: FormDataEntryValue[] = [];
		for (const [key, value] of data) {
			if (key === name) values.push(value);
		}
		return values;
	}

	has(name: string): boolean {
		const data = dataWm.get(this)!;
		for (const [key] of data) {
			if (key === name) return true;
		}
		return false;
	}

	set(name: string, value: string | Blob): void;
	set(name: string, value: string): void;
	set(name: string, blobValue: Blob, filename?: string): void;
	set(name: string, blobValue: string | Blob, filename?: string): void {
		const value =
			typeof blobValue === 'string'
				? blobValue
				: new File([blobValue], filename ?? name);
		const data = dataWm.get(this)!;
		for (const entry of data) {
			if (entry[0] === name) {
				entry[1] = value;
				return;
			}
		}
		data.push([name, value]);
	}

	forEach(
		callbackfn: (
			value: FormDataEntryValue,
			key: string,
			parent: globalThis.FormData
		) => void,
		thisArg?: any
	): void {
		for (const [key, value] of this) {
			callbackfn.call(thisArg, value, key, this);
		}
	}

	*entries(): IterableIterator<[string, FormDataEntryValue]> {
		const data = dataWm.get(this)!;
		for (const entry of data) {
			yield entry;
		}
	}

	*keys(): IterableIterator<string> {
		const data = dataWm.get(this)!;
		for (const [key] of data) {
			yield key;
		}
	}

	*values(): IterableIterator<FormDataEntryValue> {
		const data = dataWm.get(this)!;
		for (const [_, value] of data) {
			yield value;
		}
	}

	[Symbol.iterator]() {
		return this.entries();
	}
}

def('FormData', FormData);
