import { test } from '../src/tap';

// `import.meta` and dynamic-import error semantics that are comparable between
// nx.js (native ES module loader, see source/module.cc) and Chrome. Multi-file
// static/dynamic import of sibling files is verified separately on-device
// (the conformance harness injects fixture content inline, so there are no
// sibling files to resolve here).

test('import.meta.url is a non-empty string', (t) => {
	t.equal(typeof import.meta.url, 'string', 'import.meta.url is a string');
	t.ok(import.meta.url.length > 0, 'import.meta.url is non-empty');
});

test('dynamic import of a bare specifier rejects', async (t) => {
	let threw = false;
	try {
		// A bare specifier (no ./, ../, /, or scheme) is not resolvable: nx.js
		// has no node_modules/import-map semantics, and Chrome rejects an
		// unmapped bare specifier too.
		await import('this-is-a-bare-specifier-that-does-not-exist');
	} catch {
		threw = true;
	}
	t.ok(threw, 'import() of a bare specifier rejects');
});

test('dynamic import of a missing relative module rejects', async (t) => {
	let threw = false;
	try {
		await import('./this-sibling-does-not-exist.mjs');
	} catch {
		threw = true;
	}
	t.ok(threw, 'import() of a missing module rejects');
});
