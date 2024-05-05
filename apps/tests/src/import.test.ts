import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('import');

test('`import.meta.url` is a string', () => {
	assert.type(import.meta.url, 'string');

	// This should match the esbuild bundle output file name
	assert.equal(new URL(import.meta.url).href, Switch.entrypoint);
});

test('`import.meta.main` is a boolean', () => {
	assert.type(import.meta.main, 'boolean');

	// This should be `true` since esbuild is used to bundle
	assert.equal(import.meta.main, true);
});

test.run();
