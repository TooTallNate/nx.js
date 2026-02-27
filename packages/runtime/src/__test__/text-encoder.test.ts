import { describe, it, expect } from 'vitest';

describe('TextEncoder', () => {
	const bad = [
		{ input: '\uD800', expected: '\uFFFD', name: 'lone surrogate lead' },
		{ input: '\uDC00', expected: '\uFFFD', name: 'lone surrogate trail' },
		{ input: '\uD800\u0000', expected: '\uFFFD\u0000', name: 'unmatched surrogate lead' },
		{ input: '\uDC00\u0000', expected: '\uFFFD\u0000', name: 'unmatched surrogate trail' },
		{ input: '\uDC00\uD800', expected: '\uFFFD\uFFFD', name: 'swapped surrogate pair' },
		{ input: '\uD834\uDD1E', expected: '\uD834\uDD1E', name: 'properly encoded MUSICAL SYMBOL G CLEF (U+1D11E)' },
	];

	for (const t of bad) {
		it(`USVString handling: ${t.name}`, () => {
			const encoded = new TextEncoder().encode(t.input);
			const decoded = new TextDecoder().decode(encoded);
			expect(decoded).toBe(t.expected);
		});
	}

	it('USVString default', () => {
		expect(new TextEncoder().encode().length).toBe(0);
	});

	it('Encode basic', () => {
		const b = new TextEncoder().encode('hello world');
		expect(Array.from(b)).toEqual([104, 101, 108, 108, 111, 32, 119, 111, 114, 108, 100]);
	});
});
