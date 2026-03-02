/**
 * Minimal TAP 14 producer for nx.js conformance tests.
 *
 * Designed to run in:
 * - QuickJS (nxjs-test binary)
 * - Chrome (via Playwright)
 * - Nintendo Switch (apps/tests app)
 *
 * Zero dependencies. Output goes to console.log().
 *
 * Usage:
 *   import { test } from './tap';
 *
 *   test('my test', (t) => {
 *       t.equal(1 + 1, 2, 'math works');
 *   });
 *
 *   test('async test', async (t) => {
 *       const val = await Promise.resolve(42);
 *       t.equal(val, 42);
 *   });
 */

// --- Deep equality ---

function deepEqual(a: unknown, b: unknown): boolean {
	if (a === b) return true;
	if (a === null || b === null) return false;
	if (typeof a !== typeof b) return false;

	if (typeof a === 'object') {
		const aObj = a as Record<string, unknown>;
		const bObj = b as Record<string, unknown>;

		if (Array.isArray(aObj)) {
			if (!Array.isArray(bObj)) return false;
			if (aObj.length !== bObj.length) return false;
			for (let i = 0; i < aObj.length; i++) {
				if (!deepEqual(aObj[i], bObj[i])) return false;
			}
			return true;
		}

		if (aObj instanceof ArrayBuffer && bObj instanceof ArrayBuffer) {
			if (aObj.byteLength !== bObj.byteLength) return false;
			const aView = new Uint8Array(aObj);
			const bView = new Uint8Array(bObj);
			for (let i = 0; i < aView.length; i++) {
				if (aView[i] !== bView[i]) return false;
			}
			return true;
		}

		if (ArrayBuffer.isView(aObj) && ArrayBuffer.isView(bObj)) {
			const aArr = aObj as unknown as Uint8Array;
			const bArr = bObj as unknown as Uint8Array;
			if (aArr.length !== bArr.length) return false;
			for (let i = 0; i < aArr.length; i++) {
				if (aArr[i] !== bArr[i]) return false;
			}
			return true;
		}

		const aKeys = Object.keys(aObj);
		const bKeys = Object.keys(bObj);
		if (aKeys.length !== bKeys.length) return false;

		for (const key of aKeys) {
			if (!Object.prototype.hasOwnProperty.call(bObj, key)) return false;
			if (!deepEqual(aObj[key], bObj[key])) return false;
		}
		return true;
	}

	return false;
}

// --- Serialization ---

function inspect(val: unknown): string {
	if (val === undefined) return 'undefined';
	if (val === null) return 'null';
	if (typeof val === 'string') return JSON.stringify(val);
	if (typeof val === 'number' || typeof val === 'boolean') return String(val);
	if (typeof val === 'function')
		return `[Function: ${val.name || 'anonymous'}]`;
	if (typeof val === 'symbol') return val.toString();
	if (typeof val === 'bigint') return `${val}n`;
	if (Array.isArray(val)) return JSON.stringify(val);
	if (val instanceof Error) return `${val.name}: ${val.message}`;
	try {
		return JSON.stringify(val);
	} catch {
		return String(val);
	}
}

// --- Test context (the `t` object) ---

export interface TestContext {
	ok(value: unknown, msg?: string): void;
	notOk(value: unknown, msg?: string): void;
	equal(actual: unknown, expected: unknown, msg?: string): void;
	notEqual(actual: unknown, expected: unknown, msg?: string): void;
	deepEqual(actual: unknown, expected: unknown, msg?: string): void;
	throws(fn: () => unknown, expected?: RegExp | string, msg?: string): void;
	doesNotThrow(fn: () => unknown, msg?: string): void;
	match(actual: string, pattern: RegExp, msg?: string): void;
	pass(msg?: string): void;
	fail(msg?: string): void;
}

interface TestResult {
	ok: boolean;
	name: string;
	diagnostics?: string;
}

interface TestEntry {
	name: string;
	fn: (t: TestContext) => void | Promise<void>;
}

// --- Global state ---

const queue: TestEntry[] = [];
let scheduled = false;
let running = false;

// --- TAP output ---

function tapLine(line: string): void {
	console.log(line);
}

function yamlBlock(obj: Record<string, string>): string {
	const lines: string[] = ['  ---'];
	for (const [key, value] of Object.entries(obj)) {
		lines.push(`  ${key}: ${value}`);
	}
	lines.push('  ...');
	return lines.join('\n');
}

// --- Public API ---

export function test(
	name: string,
	fn: (t: TestContext) => void | Promise<void>,
): void {
	queue.push({ name, fn });

	if (!scheduled && !running) {
		scheduled = true;
		// Schedule run after current microtask queue drains
		Promise.resolve().then(() => {
			scheduled = false;
			return run();
		});
	}
}

// Allow explicit skip
test.skip = function skip(
	_name: string,
	_fn: (t: TestContext) => void | Promise<void>,
): void {
	// No-op: skipped tests are not registered
};

export async function run(): Promise<{
	total: number;
	pass: number;
	fail: number;
}> {
	running = true;
	const results: TestResult[] = [];
	let assertionCount = 0;

	tapLine('TAP version 14');

	for (const entry of queue) {
		tapLine(`# ${entry.name}`);

		const testResults: TestResult[] = [];

		function recordAssertion(
			ok: boolean,
			name: string,
			diagnostics?: string,
		): void {
			assertionCount++;
			const result: TestResult = { ok, name, diagnostics };
			testResults.push(result);
			results.push(result);

			const status = ok ? 'ok' : 'not ok';
			tapLine(`${status} ${assertionCount} - ${name}`);
			if (!ok && diagnostics) {
				tapLine(diagnostics);
			}
		}

		const t: TestContext = {
			ok(value: unknown, msg?: string) {
				const m = msg || 'should be truthy';
				if (value) {
					recordAssertion(true, m);
				} else {
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'ok',
							expected: 'truthy',
							actual: inspect(value),
						}),
					);
				}
			},

			notOk(value: unknown, msg?: string) {
				const m = msg || 'should be falsy';
				if (!value) {
					recordAssertion(true, m);
				} else {
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'notOk',
							expected: 'falsy',
							actual: inspect(value),
						}),
					);
				}
			},

			equal(actual: unknown, expected: unknown, msg?: string) {
				const m = msg || `should equal ${inspect(expected)}`;
				if (actual === expected) {
					recordAssertion(true, m);
				} else {
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'equal',
							expected: inspect(expected),
							actual: inspect(actual),
						}),
					);
				}
			},

			notEqual(actual: unknown, expected: unknown, msg?: string) {
				const m = msg || `should not equal ${inspect(expected)}`;
				if (actual !== expected) {
					recordAssertion(true, m);
				} else {
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'notEqual',
							expected: `not ${inspect(expected)}`,
							actual: inspect(actual),
						}),
					);
				}
			},

			deepEqual(actual: unknown, expected: unknown, msg?: string) {
				const m = msg || 'should deep equal';
				if (deepEqual(actual, expected)) {
					recordAssertion(true, m);
				} else {
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'deepEqual',
							expected: inspect(expected),
							actual: inspect(actual),
						}),
					);
				}
			},

			throws(fn: () => unknown, expected?: RegExp | string, msg?: string) {
				const m = msg || 'should throw';
				try {
					fn();
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'throws',
							expected: 'function to throw',
							actual: 'did not throw',
						}),
					);
				} catch (err: unknown) {
					if (expected) {
						const errMsg = err instanceof Error ? err.message : String(err);
						if (typeof expected === 'string') {
							if (errMsg.includes(expected)) {
								recordAssertion(true, m);
							} else {
								recordAssertion(
									false,
									m,
									yamlBlock({
										operator: 'throws',
										expected: inspect(expected),
										actual: inspect(errMsg),
									}),
								);
							}
						} else if (expected instanceof RegExp) {
							if (expected.test(errMsg)) {
								recordAssertion(true, m);
							} else {
								recordAssertion(
									false,
									m,
									yamlBlock({
										operator: 'throws',
										expected: String(expected),
										actual: inspect(errMsg),
									}),
								);
							}
						}
					} else {
						recordAssertion(true, m);
					}
				}
			},

			doesNotThrow(fn: () => unknown, msg?: string) {
				const m = msg || 'should not throw';
				try {
					fn();
					recordAssertion(true, m);
				} catch (err: unknown) {
					const errMsg = err instanceof Error ? err.message : String(err);
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'doesNotThrow',
							expected: 'no error',
							actual: errMsg,
						}),
					);
				}
			},

			match(actual: string, pattern: RegExp, msg?: string) {
				const m = msg || `should match ${pattern}`;
				if (pattern.test(actual)) {
					recordAssertion(true, m);
				} else {
					recordAssertion(
						false,
						m,
						yamlBlock({
							operator: 'match',
							expected: String(pattern),
							actual: inspect(actual),
						}),
					);
				}
			},

			pass(msg?: string) {
				recordAssertion(true, msg || '(pass)');
			},

			fail(msg?: string) {
				recordAssertion(
					false,
					msg || '(fail)',
					yamlBlock({
						operator: 'fail',
					}),
				);
			},
		};

		try {
			await entry.fn(t);
		} catch (err: unknown) {
			// Uncaught error in the test function itself
			assertionCount++;
			const errMsg =
				err instanceof Error ? `${err.name}: ${err.message}` : String(err);
			const result: TestResult = {
				ok: false,
				name: `uncaught error in "${entry.name}"`,
				diagnostics: yamlBlock({
					operator: 'error',
					message: errMsg,
				}),
			};
			results.push(result);
			tapLine(`not ok ${assertionCount} - ${result.name}`);
			tapLine(result.diagnostics!);
		}
	}

	// Plan line
	tapLine(`1..${assertionCount}`);

	// Summary comments
	const pass = results.filter((r) => r.ok).length;
	const fail = results.filter((r) => !r.ok).length;
	tapLine(`# tests ${assertionCount}`);
	tapLine(`# pass ${pass}`);
	tapLine(`# fail ${fail}`);

	running = false;

	const summary = { total: assertionCount, pass, fail };

	// Signal completion — used by Playwright to know when TAP output is done
	(globalThis as any).__tapDone = true;

	// Exit the nxjs-test event loop if available
	if (typeof (globalThis as any).Switch !== 'undefined') {
		const sw = (globalThis as any).Switch;
		if (sw && typeof sw.exit === 'function') {
			sw.exit();
		}
	}

	return summary;
}
