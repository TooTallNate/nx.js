// uvu/assert-compatible assertions used by the nx.js test suites.
// Implements the subset the suites use: equal, is, not, ok, throws, type,
// instance. `equal` is a deep structural comparison (like uvu); `is` is strict
// reference/`===` equality.

export class Assertion extends Error {
	constructor(message: string) {
		super(message);
		this.name = 'Assertion';
	}
}

function stringify(v: unknown): string {
	if (typeof v === 'string') return JSON.stringify(v);
	if (typeof v === 'bigint') return `${v}n`;
	if (v === undefined) return 'undefined';
	if (typeof v === 'function') return `[Function: ${v.name || 'anonymous'}]`;
	try {
		return JSON.stringify(v);
	} catch {
		return String(v);
	}
}

function deepEqual(a: unknown, b: unknown): boolean {
	if (a === b) return true;
	if (typeof a !== typeof b) return false;
	if (a === null || b === null) return a === b;
	if (typeof a !== 'object') return a === b;

	// Array
	const aIsArr = Array.isArray(a);
	const bIsArr = Array.isArray(b);
	if (aIsArr || bIsArr) {
		if (!aIsArr || !bIsArr) return false;
		const aa = a as unknown[];
		const bb = b as unknown[];
		if (aa.length !== bb.length) return false;
		for (let i = 0; i < aa.length; i++) {
			if (!deepEqual(aa[i], bb[i])) return false;
		}
		return true;
	}

	// TypedArray / ArrayBuffer views
	if (ArrayBuffer.isView(a) && ArrayBuffer.isView(b)) {
		const ua = new Uint8Array(
			(a as any).buffer,
			(a as any).byteOffset,
			(a as any).byteLength,
		);
		const ub = new Uint8Array(
			(b as any).buffer,
			(b as any).byteOffset,
			(b as any).byteLength,
		);
		if (ua.length !== ub.length) return false;
		for (let i = 0; i < ua.length; i++) {
			if (ua[i] !== ub[i]) return false;
		}
		return true;
	}

	// Plain objects
	const aKeys = Object.keys(a as object);
	const bKeys = Object.keys(b as object);
	if (aKeys.length !== bKeys.length) return false;
	for (const k of aKeys) {
		if (!Object.prototype.hasOwnProperty.call(b, k)) return false;
		if (!deepEqual((a as any)[k], (b as any)[k])) return false;
	}
	return true;
}

export function ok(value: unknown, msg?: string): void {
	if (!value) {
		throw new Assertion(msg || `Expected value to be truthy`);
	}
}

export function is(actual: unknown, expected: unknown, msg?: string): void {
	if (actual !== expected) {
		throw new Assertion(
			msg ||
				`Expected ${stringify(actual)} to strictly equal ${stringify(expected)}`,
		);
	}
}

export function equal(actual: unknown, expected: unknown, msg?: string): void {
	if (!deepEqual(actual, expected)) {
		throw new Assertion(
			msg ||
				`Expected ${stringify(actual)} to deeply equal ${stringify(expected)}`,
		);
	}
}

// uvu's `assert.not` is a callable (asserts falsy) that ALSO carries
// sub-assertions like `assert.not.equal`, `assert.not.type`, etc. Model that
// with a function object.
interface NotAssert {
	(actual: unknown, msg?: string): void;
	equal(actual: unknown, expected: unknown, msg?: string): void;
	type(value: unknown, expected: string, msg?: string): void;
	instance(value: unknown, ctor: Function, msg?: string): void;
	ok(value: unknown, msg?: string): void;
}

const notFn = ((actual: unknown, msg?: string): void => {
	// assert.not(value) asserts the value is falsy.
	if (actual) {
		throw new Assertion(msg || `Expected value to be falsy`);
	}
}) as NotAssert;

notFn.equal = (actual: unknown, expected: unknown, msg?: string): void => {
	if (deepEqual(actual, expected)) {
		throw new Assertion(
			msg ||
				`Expected ${stringify(actual)} NOT to deeply equal ${stringify(expected)}`,
		);
	}
};
notFn.type = (value: unknown, expected: string, msg?: string): void => {
	if (typeof value === expected) {
		throw new Assertion(msg || `Expected typeof NOT to be "${expected}"`);
	}
};
notFn.instance = (value: unknown, ctor: Function, msg?: string): void => {
	if (value instanceof ctor) {
		throw new Assertion(
			msg || `Expected value NOT to be an instance of ${ctor.name}`,
		);
	}
};
notFn.ok = (value: unknown, msg?: string): void => {
	if (value) {
		throw new Assertion(msg || `Expected value to be falsy`);
	}
};

export const not = notFn;

export function type(value: unknown, expected: string, msg?: string): void {
	const actual = typeof value;
	if (actual !== expected) {
		throw new Assertion(
			msg || `Expected typeof to be "${expected}" but got "${actual}"`,
		);
	}
}

export function instance(
	value: unknown,
	ctor: Function,
	msg?: string,
): void {
	if (!(value instanceof ctor)) {
		throw new Assertion(
			msg || `Expected value to be an instance of ${ctor.name}`,
		);
	}
}

export function throws(
	fn: () => unknown,
	expected?: RegExp | ((err: any) => boolean) | string,
	msg?: string,
): void {
	let threw = false;
	let caught: any;
	try {
		fn();
	} catch (err) {
		threw = true;
		caught = err;
	}
	if (!threw) {
		throw new Assertion(msg || `Expected function to throw`);
	}
	if (expected instanceof RegExp) {
		if (!expected.test(caught?.message ?? String(caught))) {
			throw new Assertion(
				msg || `Expected error message to match ${expected}`,
			);
		}
	} else if (typeof expected === 'function') {
		if (!expected(caught)) {
			throw new Assertion(msg || `Expected error to satisfy predicate`);
		}
	} else if (typeof expected === 'string') {
		if ((caught?.message ?? String(caught)) !== expected) {
			throw new Assertion(
				msg || `Expected error message to be "${expected}"`,
			);
		}
	}
}
