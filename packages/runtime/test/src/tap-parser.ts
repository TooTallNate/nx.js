/**
 * Minimal TAP output parser.
 *
 * Parses TAP 14 output from either nxjs-test or Chrome into a structured
 * result that vitest can compare.
 */

export interface TapAssertion {
	number: number;
	ok: boolean;
	name: string;
	diagnostics?: Record<string, string>;
}

export interface TapResult {
	version: number;
	assertions: TapAssertion[];
	plan: { start: number; end: number } | null;
	summary: {
		tests: number;
		pass: number;
		fail: number;
	};
	comments: string[];
}

export function parseTap(output: string): TapResult {
	const lines = output.split('\n');
	const result: TapResult = {
		version: 14,
		assertions: [],
		plan: null,
		summary: { tests: 0, pass: 0, fail: 0 },
		comments: [],
	};

	let inYaml = false;
	let currentDiagnostics: Record<string, string> = {};

	for (const line of lines) {
		const trimmed = line.trim();

		// TAP version
		if (trimmed.startsWith('TAP version ')) {
			result.version = parseInt(trimmed.slice(12), 10);
			continue;
		}

		// Plan line (1..N)
		const planMatch = trimmed.match(/^(\d+)\.\.(\d+)$/);
		if (planMatch) {
			result.plan = {
				start: parseInt(planMatch[1], 10),
				end: parseInt(planMatch[2], 10),
			};
			continue;
		}

		// YAML diagnostics block
		if (trimmed === '---') {
			inYaml = true;
			currentDiagnostics = {};
			continue;
		}
		if (trimmed === '...') {
			inYaml = false;
			// Attach to the last assertion
			if (result.assertions.length > 0) {
				result.assertions[result.assertions.length - 1].diagnostics =
					currentDiagnostics;
			}
			continue;
		}
		if (inYaml) {
			const kvMatch = trimmed.match(/^(\w+):\s*(.*)$/);
			if (kvMatch) {
				currentDiagnostics[kvMatch[1]] = kvMatch[2];
			}
			continue;
		}

		// ok / not ok lines
		const assertMatch = trimmed.match(/^(ok|not ok)\s+(\d+)\s*(?:-\s*(.*))?$/);
		if (assertMatch) {
			result.assertions.push({
				number: parseInt(assertMatch[2], 10),
				ok: assertMatch[1] === 'ok',
				name: (assertMatch[3] || '').trim(),
			});
			continue;
		}

		// Comment lines
		if (trimmed.startsWith('# ')) {
			const comment = trimmed.slice(2);

			// Parse summary comments
			const testsMatch = comment.match(/^tests\s+(\d+)$/);
			if (testsMatch) {
				result.summary.tests = parseInt(testsMatch[1], 10);
				continue;
			}
			const passMatch = comment.match(/^pass\s+(\d+)$/);
			if (passMatch) {
				result.summary.pass = parseInt(passMatch[1], 10);
				continue;
			}
			const failMatch = comment.match(/^fail\s+(\d+)$/);
			if (failMatch) {
				result.summary.fail = parseInt(failMatch[1], 10);
				continue;
			}

			result.comments.push(comment);
		}
	}

	return result;
}
