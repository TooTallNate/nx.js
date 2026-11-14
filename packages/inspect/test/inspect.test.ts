import { describe, expect, test } from 'vitest';
import { createInspect } from '../src/index';

const inspect = createInspect();
const strip = (s: string) => s.replace(/\x1b\[[0-9;]*m/g, '');
const i = (v: unknown) => strip(inspect(v));

describe('inspect', () => {
	test('repeated references that are not cycles are printed in full', () => {
		const a = { x: 1 };
		expect(i([a, a])).toBe('[ { x: 1 }, { x: 1 } ]');
		expect(i({ p: a, q: a })).toBe('{ p: { x: 1 }, q: { x: 1 } }');
		expect(i([[a], { b: a }])).toBe('[ [ { x: 1 } ], { b: { x: 1 } } ]');
	});

	test('cycles are still marked as circular', () => {
		const o: Record<string, unknown> = { x: 1 };
		o.self = o;
		expect(i(o)).toBe('{ x: 1, self: [Circular *1] }');

		const arr: unknown[] = [];
		arr.push(arr);
		expect(i(arr)).toBe('[ [Circular *1] ]');

		const a: Record<string, unknown> = {};
		const b = { a };
		a.b = b;
		expect(i([a, a])).toBe(
			'[ { b: { a: [Circular *2] } }, { b: { a: [Circular *2] } } ]',
		);
	});

	test('invalid Date', () => {
		expect(i(new Date(NaN))).toBe('Invalid Date');
		expect(i(new Date(0))).toBe('1970-01-01T00:00:00.000Z');
	});
});
