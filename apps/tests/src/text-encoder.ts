import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('TextEncoder');

var bad = [
	{
		input: '\uD800',
		expected: '\uFFFD',
		name: 'lone surrogate lead',
	},
	{
		input: '\uDC00',
		expected: '\uFFFD',
		name: 'lone surrogate trail',
	},
	{
		input: '\uD800\u0000',
		expected: '\uFFFD\u0000',
		name: 'unmatched surrogate lead',
	},
	{
		input: '\uDC00\u0000',
		expected: '\uFFFD\u0000',
		name: 'unmatched surrogate trail',
	},
	{
		input: '\uDC00\uD800',
		expected: '\uFFFD\uFFFD',
		name: 'swapped surrogate pair',
	},
	{
		input: '\uD834\uDD1E',
		expected: '\uD834\uDD1E',
		name: 'properly encoded MUSICAL SYMBOL G CLEF (U+1D11E)',
	},
];

for (const t of bad) {
	test('USVString handling: ' + t.name, function () {
		var encoded = new TextEncoder().encode(t.input);
		var decoded = new TextDecoder().decode(encoded);
		assert.equal(decoded, t.expected);
	});
}

test('USVString default', function () {
	assert.equal(
		new TextEncoder().encode().length,
		0,
		'Should default to empty string',
	);
});

test('Encode basic', () => {
	const b = new TextEncoder().encode('hello world');
	assert.equal(
		Array.from(b),
		[104, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100]
	);
});

test.run();
