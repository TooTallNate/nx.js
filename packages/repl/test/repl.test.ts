import { $ } from 'kleur/colors';
$.enabled = true;

import { createInspect } from '@nx.js/inspect';
import { describe, expect, test } from 'vitest';
import { REPL, type REPLOptions } from '../src/index';

const encoder = new TextEncoder();

const opts: REPLOptions = {
	inspect: createInspect(),
};

describe('REPL', () => {
	test('Renders empty prompt', async () => {
		const stream = new TextDecoderStream();
		const repl = new REPL(stream.writable.getWriter(), opts);
		const reader = stream.readable.getReader();

		repl.renderPrompt();
		const { value } = await reader.read();
		expect(value).toMatchInlineSnapshot(`
          "
          [2K[1m[36m> [39m[22m[1m[43m [49m[22m"
        `);
	});

	test('Echoes user input', async () => {
		const stream = new TextDecoderStream();
		const repl = new REPL(stream.writable.getWriter(), opts);
		const reader = stream.readable.getReader();

		repl.write(encoder.encode('hello world'));
		const { value } = await reader.read();
		expect(value).toMatchInlineSnapshot(`
          "
          [2K[1m[36m> [39m[22mhello world[1m[43m [49m[22m"
        `);
	});

	describe('submit()', () => {
		test('Evaluates user input', async () => {
			const stream = new TextDecoderStream();
			const repl = new REPL(stream.writable.getWriter(), opts);
			const reader = stream.readable.getReader();

			repl.write(encoder.encode('1 + 1'));
			let chunk = await reader.read();
			expect(chunk.value).toMatchInlineSnapshot(`
          "
          [2K[1m[36m> [39m[22m1 + 1[1m[43m [49m[22m"
        `);

			repl.submit();

			chunk = await reader.read();
			expect(chunk.value).toMatchInlineSnapshot(`
        "
        [2K[1m[36m> [39m[22m1 + 1
        "
      `);

			chunk = await reader.read();
			expect(chunk.value).toMatchInlineSnapshot(`
				"[1m[33m2[39m[22m

				"
			`);

			expect(globalThis._).toBe(2);
		});
	});
});
