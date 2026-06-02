// Minimal uvu-compatible test harness for the nx.js on-device test app.
//
// Why not uvu? uvu auto-runs each suite on the microtask queue and writes its
// own console formatting + calls process.exit, which makes it impossible to
// (a) run tests in a deterministic, awaited order and (b) reliably tee a
// per-test marker to the SD card so a hang/crash pinpoints the exact test.
//
// This harness instead REGISTERS tests into a global ordered queue; `runAll()`
// (called from main.ts) executes them sequentially, awaiting async tests, and
// reports via a pluggable reporter that writes to a file after every line.

export type TestFn = () => void | Promise<void>;

export interface RegisteredTest {
	suite: string;
	name: string;
	fn: TestFn;
	skip: boolean;
}

// Global ordered registry shared across all imported *.test.ts files.
const registry: RegisteredTest[] = [];

export interface Suite {
	(name: string, fn: TestFn): void;
	skip(name: string, fn?: TestFn): void;
	// uvu compatibility: `test.run()` is a no-op here (the central runner in
	// main.ts drives execution). Kept so the existing .test.ts files compile
	// unchanged.
	run(): void;
}

export function suite(suiteName: string): Suite {
	const add =
		(skip: boolean) =>
		(name: string, fn?: TestFn) => {
			registry.push({
				suite: suiteName,
				name,
				fn: fn ?? (() => {}),
				skip,
			});
		};
	const test = add(false) as Suite;
	test.skip = add(true);
	test.run = () => {};
	return test;
}

export function getRegistry(): RegisteredTest[] {
	return registry;
}

export interface RunResult {
	total: number;
	passed: number;
	failed: number;
	skipped: number;
}

export interface Reporter {
	onStart?(total: number): void;
	onSuite?(suite: string): void;
	onTestStart(suite: string, name: string): void;
	onTestPass(suite: string, name: string, ms: number): void;
	onTestFail(suite: string, name: string, err: unknown): void;
	onTestSkip(suite: string, name: string): void;
	onDone(result: RunResult): void;
}

// Per-test timeout (ms). A test that doesn't settle within this window is
// recorded as a failure so the run can continue and produce a full report.
const TEST_TIMEOUT_MS = 15_000;

function withTimeout<T>(p: T | Promise<T>, ms: number): Promise<T> {
	return new Promise<T>((resolve, reject) => {
		let done = false;
		const timer = setTimeout(() => {
			if (done) return;
			done = true;
			reject(new Error(`timed out after ${ms}ms`));
		}, ms);
		Promise.resolve(p).then(
			(v) => {
				if (done) return;
				done = true;
				clearTimeout(timer);
				resolve(v);
			},
			(err) => {
				if (done) return;
				done = true;
				clearTimeout(timer);
				reject(err);
			},
		);
	});
}

// Run every registered test sequentially, awaiting async tests. The reporter
// is notified before/after each test so a hang leaves a clear trail.
export async function runAll(reporter: Reporter): Promise<RunResult> {
	const tests = registry;
	const result: RunResult = {
		total: tests.length,
		passed: 0,
		failed: 0,
		skipped: 0,
	};
	reporter.onStart?.(tests.length);

	let currentSuite = '';
	for (const t of tests) {
		if (t.suite !== currentSuite) {
			currentSuite = t.suite;
			reporter.onSuite?.(currentSuite);
		}
		if (t.skip) {
			result.skipped++;
			reporter.onTestSkip(t.suite, t.name);
			continue;
		}
		reporter.onTestStart(t.suite, t.name);
		const start = Date.now();
		try {
			// Race the test against a timeout so a hanging async test is
			// recorded as a FAIL and the run continues (giving a full report in
			// one pass instead of stalling on the first hang). The pre-flushed
			// "RUN" marker still pinpoints the offender if the process is
			// killed before the timeout fires.
			await withTimeout(t.fn(), TEST_TIMEOUT_MS);
			result.passed++;
			reporter.onTestPass(t.suite, t.name, Date.now() - start);
		} catch (err) {
			result.failed++;
			reporter.onTestFail(t.suite, t.name, err);
		}
	}
	reporter.onDone(result);
	return result;
}
