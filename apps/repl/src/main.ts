import { cursor, erase } from 'sisteransi';
import { bold, cyan, bgYellow } from 'kleur/colors';
import { inspect } from './inspect';

console.log('Welcome to the nx.js REPL!');
console.log('Use a keyboard to enter JavaScript code.');
console.log('Press the + button or the "Esc" key to exit...');
console.log();

let buffer = '';
let historyIndex = 0;
let cursorPosition = 0;
const history: string[] = [];
const prompt = bold(cyan('>'));

const cursorChar = (v: string) => bold(bgYellow(v || ' '));

function renderPrompt() {
	const bufferL = buffer.slice(0, cursorPosition);
	const bufferP = buffer[cursorPosition];
	const bufferR = buffer.slice(cursorPosition + 1);
	Switch.print(
		`\r${erase.line}${prompt} ${bufferL}${cursorChar(bufferP)}${bufferR}`
	);
}

Switch.addEventListener('keydown', (e) => {
	const { key } = e;
	if (key.length === 1) {
		// Printable character
		buffer += key;
		cursorPosition++;
		renderPrompt();
	} else if (key === 'Enter') {
		// Remove cursor
		Switch.print(`${cursor.backward(1)}${erase.lineEnd}\n`);
		try {
			history.push(buffer);
			historyIndex = history.length - 1;
			const trimmed = buffer.trim();
			const result =
				trimmed.length === 0 ? undefined : eval(`(${trimmed})`);
			buffer = '';
			cursorPosition = 0;
			Switch.print(`${inspect(result)}\n\n`);
		} catch (err: unknown) {
			buffer = '';
			cursorPosition = 0;
			if (err instanceof Error) {
				console.log(`${err}\n${err.stack}`);
			} else {
				console.log(`Error: ${err}`);
			}
		}
		renderPrompt();
	} else if (key === 'Backspace') {
		if (buffer.length) {
			buffer = buffer.slice(0, -1);
			cursorPosition--;
			renderPrompt();
		}
	} else if (key === 'Escape') {
		Switch.exit();
	} else if (key === 'ArrowUp') {
	} else if (key === 'ArrowDown') {
	} else if (key === 'ArrowLeft') {
		if (cursorPosition > 0) {
			cursorPosition--;
			renderPrompt();
		}
	} else if (key === 'ArrowRight') {
		if (cursorPosition < buffer.length) {
			cursorPosition++;
			renderPrompt();
		}
	}
});

renderPrompt();
