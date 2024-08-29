import util from 'node:util';
import { describe, expect, test } from 'vitest';
import { REPL } from '../src/repl';

const encoder = new TextEncoder();

class NodeREPL extends REPL {
	inspect(v: unknown): string {
		return util.inspect(v);
	}
	escape(): void {}
}

describe('REPL', () => {
	test('Renders empty prompt', async () => {
		const stream = new TextDecoderStream();
		const repl = new NodeREPL(stream.writable.getWriter());
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
		const repl = new NodeREPL(stream.writable.getWriter());
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
			const repl = new NodeREPL(stream.writable.getWriter());
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
        "2

        "
      `);

			expect(globalThis._).toBe(2);
		});
	});
});
