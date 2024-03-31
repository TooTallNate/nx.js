export class Deferred<T> {
	promise: Promise<T>;
	resolve!: (v: T) => void;
	reject!: (v: any) => void;

	constructor() {
		this.promise = new Promise((r, j) => {
			this.resolve = r;
			this.reject = j;
		});
	}
}
