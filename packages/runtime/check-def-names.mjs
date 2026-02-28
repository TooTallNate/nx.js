#!/usr/bin/env node

/**
 * Checks the bundled runtime.js for `def()` calls where the first argument
 * (class/function name) ends with a digit — a sign that esbuild renamed it
 * due to a naming conflict (e.g. `TextEncoder` → `TextEncoder2`).
 *
 * This is a problem because `def()` without a second argument uses `.name`
 * to register the global, so a renamed class gets registered under the
 * wrong name (e.g. `globalThis.TextEncoder2` instead of `globalThis.TextEncoder`).
 *
 * Calls that provide an explicit second argument (the name string) are exempt,
 * since the rename doesn't affect the registered global name.
 */

import fs from 'fs';

const runtimePath = new URL('runtime.js', import.meta.url);
const source = fs.readFileSync(runtimePath, 'utf-8');

// Match def(<identifier>) and def(<identifier>, <optional second arg>)
// The def() calls in the bundle look like: def(ClassName) or def(ClassName, "name")
const defPattern = /\bdef\((\w+)(?:\s*,\s*("[^"]*"|'[^']*'))?\)/g;

let match;
const errors = [];

while ((match = defPattern.exec(source)) !== null) {
	const identifier = match[1];
	const explicitName = match[2];

	// If an explicit name string is provided, the rename is harmless
	if (explicitName) continue;

	// Check if the identifier ends with a digit
	if (/\d$/.test(identifier)) {
		// Find line number
		const upToMatch = source.slice(0, match.index);
		const line = upToMatch.split('\n').length;
		errors.push({ identifier, line });
	}
}

if (errors.length > 0) {
	console.error(
		'ERROR: Found def() calls with renamed identifiers (esbuild class renaming detected):\n',
	);
	for (const { identifier, line } of errors) {
		console.error(
			`  runtime.js:${line}: def(${identifier}) — identifier ends with a digit`,
		);
	}
	console.error(
		'\nThis means a global reference was used instead of importing from the source file.',
	);
	console.error(
		'Fix: import the class/function directly from its definition module.\n',
	);
	process.exit(1);
} else {
	console.log(
		'✓ All def() calls use correctly named identifiers (no esbuild renames detected)',
	);
}
