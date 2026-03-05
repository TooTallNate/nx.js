import { def } from '../utils';
import type { Blob } from './blob';
import { File } from './file';

export type FormDataEntryValue = string | File;

export class FormData implements globalThis.FormData {
	#entries: [string, FormDataEntryValue][] = [];

	constructor() {}

	append(name: string, value: string | Blob): void;
	append(name: string, value: string): void;
	append(name: string, blobValue: Blob, filename?: string): void;
	append(name: string, blobValue: string | Blob, filename?: string): void {
		const value =
			typeof blobValue === 'string' || blobValue instanceof File
				? blobValue
				: new File([blobValue], filename ?? 'blob', { type: blobValue.type });
		this.#entries.push([name, value]);
	}

	delete(name: string): void {
		this.#entries = this.#entries.filter((entry) => entry[0] !== name);
	}

	get(name: string): FormDataEntryValue | null {
		for (const [key, value] of this.#entries) {
			if (key === name) return value;
		}
		return null;
	}

	getAll(name: string): FormDataEntryValue[] {
		const values: FormDataEntryValue[] = [];
		for (const [key, value] of this.#entries) {
			if (key === name) values.push(value);
		}
		return values;
	}

	has(name: string): boolean {
		for (const [key] of this.#entries) {
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
				: new File([blobValue], filename ?? 'blob', { type: blobValue.type });
		const data = this.#entries;
		let firstIndex = -1;
		for (let i = 0; i < data.length; i++) {
			if (data[i][0] === name) {
				if (firstIndex === -1) {
					firstIndex = i;
					data[i][1] = value;
				} else {
					data.splice(i, 1);
					i--;
				}
			}
		}
		if (firstIndex === -1) {
			data.push([name, value]);
		}
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

	*entries(): FormDataIterator<[string, FormDataEntryValue]> {
		for (const entry of this.#entries) {
			yield entry;
		}
	}

	*keys(): FormDataIterator<string> {
		for (const [key] of this.#entries) {
			yield key;
		}
	}

	*values(): FormDataIterator<FormDataEntryValue> {
		for (const [_k, value] of this.#entries) {
			yield value;
		}
	}

	[Symbol.iterator](): FormDataIterator<[string, FormDataEntryValue]> {
		return this.entries();
	}
}

def(FormData);
