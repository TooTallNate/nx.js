// On-device test runner for nx.js.
//
// Test files register their tests (via ./harness `suite`, aliased to `uvu`)
// into a global ordered queue; we then run them sequentially and write results
// to `sdmc:/switch/tests-results.log` synchronously after every line, so a hang
// or crash still leaves a complete partial log inspectable over FTP.

import { getRegistry, runAll, type Reporter } from './harness';

// Importing a *.test.ts file registers its tests (no auto-run; harness.run()
// is a no-op). Order here is the execution order.
import './canvas.test';
import './dommatrix.test';
import './error.test';
import './event-target.test';
import './form-data.test';
import './import.test';
import './navigator.test';
import './storage.test';
import './switch.test';
import './text-encoder.test';
import './window.test';
import './url.test';
import './uint8array.test';
import './webcrypto-aes-xts.test';
import './websocket.test';
// fetch + websocket are network-dependent; keep fetch last-ish so earlier
// suites are fully recorded before any network hang.
import './fetch.test';

const RESULTS_PATH = 'sdmc:/switch/tests-results.log';
// Only the un-flushed tail is held in memory and APPENDED to the file, so the
// in-memory buffer never grows unbounded (the previous full-rewrite approach
// kept the entire log as a growing V8 string + re-serialized it every flush,
// which pressured the heap on long runs).
let pending = '';
let pendingCount = 0;

function flush() {
	if (!pending) return;
	try {
		Switch.appendFileSync(RESULTS_PATH, pending);
		pending = '';
		pendingCount = 0;
	} catch {
		// nothing useful to do
	}
}

// `force` flushes immediately (used for FAIL / suite boundaries / RUN markers
// so a hang or crash still leaves the offending test as the last line in the
// file). Otherwise passing `ok` lines are batched.
// `show` mirrors the line to the framebuffer console (visible on-screen) — used
// sparingly (suites, failures, summary) to avoid per-test console spam.
function line(s: string, force = false, show = false) {
	pending += s + '\n';
	pendingCount++;
	if (show) {
		console.log(s);
	}
	if (force || pendingCount >= 25) {
		flush();
	}
}

function fmtErr(err: unknown): string {
	if (err && typeof err === 'object') {
		const e = err as any;
		// V8's error.stack here often omits the "Name: message" header line, so
		// prepend it explicitly to make the actual error visible (not just the
		// stack frames).
		const head = `${e.name ?? 'Error'}: ${e.message ?? ''}`.trim();
		if (typeof e.stack === 'string' && e.stack.length) {
			return e.stack.startsWith(e.name ?? 'Error')
				? e.stack
				: `${head}\n${e.stack}`;
		}
		return head || String(err);
	}
	return String(err);
}

const reporter: Reporter = {
	onStart(total) {
		line(`=== nx.js tests: ${total} tests registered ===`, true, true);
	},
	onSuite(suite) {
		line(``);
		line(`# ${suite}`, true, true); // show suite headers on screen
	},
	onTestStart(_suite, name) {
		// Written + flushed BEFORE the test runs: if the run hangs or crashes,
		// this is the last line in the file and pinpoints the offending test.
		line(`  RUN  ${name}`, true);
	},
	onTestPass(_suite, name, ms) {
		line(`  ok   ${name} (${ms}ms)`); // batched flush, file only
	},
	onTestFail(_suite, name, err) {
		line(`  FAIL ${name}`, true, true); // show failures on screen
		line(`       ${fmtErr(err).split('\n').join('\n       ')}`, true);
	},
	onTestSkip(_suite, name) {
		line(`  skip ${name}`);
	},
	onDone(r) {
		line(``);
		line(
			`=== done: ${r.passed} passed, ${r.failed} failed, ${r.skipped} skipped (of ${r.total}) ===`,
			true,
			true,
		);
	},
};

// Surface uncaught errors / unhandled rejections (e.g. from a hung async test
// that later rejects) into the log too.
addEventListener('error', (e: any) => {
	line(`[uncaught] ${fmtErr(e?.error ?? e?.message ?? e)}`);
});
addEventListener('unhandledrejection', (e: any) => {
	line(`[unhandledrejection] ${fmtErr(e?.reason)}`);
});

// Truncate the results file and write the header (subsequent writes append).
try {
	Switch.writeFileSync(
		RESULTS_PATH,
		`=== nx.js tests started ${new Date().toISOString()} ===\n`,
	);
} catch {}
line(`registered ${getRegistry().length} tests across suites`, true, true);

runAll(reporter).then((r) => {
	line(
		`exit: ${r.failed === 0 ? 'SUCCESS' : 'FAILURE'} (${r.failed} failed)`,
		true,
		true,
	);
});
