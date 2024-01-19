import { erase, cursor } from 'sisteransi';
import { bold, cyan, bgYellow } from 'kleur/colors';

const decoder = new TextDecoder();
const encoder = new TextEncoder();
const cursorChar = (v: string) => bold(bgYellow(v || ' '));

export class REPL {
	buffer = '';
	historyIndex = 0;
	cursorPosition = 0;
	history: string[] = [];
	prompt = bold(cyan('> '));
	writer: WritableStreamDefaultWriter<Uint8Array>;

	constructor(writer: WritableStreamDefaultWriter<Uint8Array>) {
		this.writer = writer;
	}

	async renderPrompt() {
		let b = this.buffer;
		if (this.cursorPosition >= 0) {
			const bufferL = this.buffer.slice(0, this.cursorPosition);
			const bufferP = this.buffer[this.cursorPosition];
			const bufferR = this.buffer.slice(this.cursorPosition + 1);
			b = `${bufferL}${cursorChar(bufferP)}${bufferR}`;
		}
		await this.print(`\r${erase.line}${this.prompt}${b}`);
	}

	async print(str: string) {
		await this.writer.write(
			encoder.encode(str.replace(/(?:\r\n|\n)/g, '\r\n'))
		);
	}

	async write(data: Uint8Array) {
		this.buffer = `${this.buffer.slice(
			0,
			this.cursorPosition
		)}${decoder.decode(data)}${this.buffer.slice(this.cursorPosition)}`;
		this.cursorPosition += data.length;
		await this.renderPrompt();
	}

	async submit() {
		// Remove cursor
		this.cursorPosition = -1;
		await this.renderPrompt();
		await this.print('\n');
		try {
			this.history.push(this.buffer);
			this.historyIndex = this.history.length;
			const trimmed = this.buffer.trim();
			let result: any = undefined;
			if (trimmed.length > 0) {
				try {
					result = (0, eval)(`(${trimmed})`);
				} catch (err: unknown) {
					if (err instanceof SyntaxError) {
						result = (0, eval)(trimmed);
					} else {
						throw err;
					}
				}
			}
			this.buffer = '';
			this.cursorPosition = 0;
			await this.print(`${Switch.inspect(result)}\n\n`);
			// @ts-expect-error `_` is not defined on `globalThis`
			globalThis._ = result;
		} catch (err: unknown) {
			this.buffer = '';
			this.cursorPosition = 0;
			if (err instanceof Error) {
				await this.print(`${err}\n${err.stack}\n`);
			} else {
				await this.print(`Error: ${err}\n`);
			}
		}
		await this.renderPrompt();
	}

	async backspace() {
		if (this.buffer.length) {
			this.buffer = `${this.buffer.slice(
				0,
				this.cursorPosition - 1
			)}${this.buffer.slice(this.cursorPosition)}`;
			this.cursorPosition--;
			await this.renderPrompt();
		}
	}

	async arrowUp() {
		if (this.historyIndex > 0) {
			this.buffer = this.history[--this.historyIndex];
			this.cursorPosition = this.buffer.length;
			await this.renderPrompt();
		}
	}

	async arrowDown() {
		if (this.historyIndex < this.history.length) {
			this.buffer = this.history[++this.historyIndex] ?? '';
			this.cursorPosition = this.buffer.length;
			await this.renderPrompt();
		}
	}

	async arrowLeft() {
		if (this.cursorPosition > 0) {
			this.cursorPosition--;
			await this.renderPrompt();
		}
	}

	async arrowRight() {
		if (this.cursorPosition < this.buffer.length) {
			this.cursorPosition++;
			await this.renderPrompt();
		}
	}

	escape() {
		Switch.exit();
	}
}
