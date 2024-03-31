import { createInternal, def } from '../utils';
import { File } from './file';
import type { Blob } from './blob';

const _ = createInternal<FormData, [string, FormDataEntryValue][]>();

export type FormDataEntryValue = string | File;

export class FormData implements globalThis.FormData {
	constructor() {
		_.set(this, []);
	}

	append(name: string, value: string | Blob): void;
	append(name: string, value: string): void;
	append(name: string, blobValue: Blob, filename?: string): void;
	append(name: string, blobValue: string | Blob, filename?: string): void {
		const value =
			typeof blobValue === 'string' || blobValue instanceof File
				? blobValue
				: new File([blobValue], filename ?? name, blobValue);
		_(this).push([name, value]);
	}

	delete(name: string): void {
		const updated = _(this).filter((entry) => entry[0] !== name);
		_.set(this, updated);
	}

	get(name: string): FormDataEntryValue | null {
		for (const [key, value] of _(this)) {
			if (key === name) return value;
		}
		return null;
	}

	getAll(name: string): FormDataEntryValue[] {
		const values: FormDataEntryValue[] = [];
		for (const [key, value] of _(this)) {
			if (key === name) values.push(value);
		}
		return values;
	}

	has(name: string): boolean {
		for (const [key] of _(this)) {
			if (key === name) return true;
		}
		return false;
	}

	set(name: string, value: string | Blob): void;
	set(name: string, value: string): void;
	set(name: string, blobValue: Blob, filename?: string): void;
	set(name: string, blobValue: string | Blob, filename?: string): void {
		const value =
			typeof blobValue === 'string' || blobValue instanceof File
				? blobValue
				: new File([blobValue], filename ?? name, blobValue);
		const data = _(this);
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
			parent: FormData,
		) => void,
		thisArg?: any,
	): void {
		for (const [key, value] of this) {
			callbackfn.call(thisArg, value, key, this);
		}
	}

	*entries(): IterableIterator<[string, FormDataEntryValue]> {
		for (const entry of _(this)) {
			yield entry;
		}
	}

	*keys(): IterableIterator<string> {
		for (const [key] of _(this)) {
			yield key;
		}
	}

	*values(): IterableIterator<FormDataEntryValue> {
		for (const [_k, value] of _(this)) {
			yield value;
		}
	}

	[Symbol.iterator]() {
		return this.entries();
	}
}

def(FormData);
