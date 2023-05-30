import { cyan, yellow } from 'kleur/colors';
import { cursor, erase } from 'sisteransi';

console.log('Welcome to the nx.js REPL!');
console.log('Use a keyboard to enter JavaScript code.');
console.log('Press the + button or the "Esc" key to exit...');
console.log();

let buffer = '';
const history: string[] = [];

const cursorChar = yellow('|');

function showPrompt() {
	Switch.print(`${cyan('>')} ${cursorChar}`);
}

Switch.addEventListener('keydown', (e) => {
	const { key } = e;
	if (key.length === 1) {
		// Printable character
		buffer += key;
		Switch.print(
			`${cursor.backward(1)}${erase.lineEnd}${key}${cursorChar}`
		);
	} else if (key === 'Enter') {
		// Remove cursor
		Switch.print(`${cursor.backward(1)}${erase.lineEnd}\n`);
		try {
			history.push(buffer);
			const result = eval(buffer);
			buffer = '';
			console.log(result);
			console.log();
		} catch (err: unknown) {
			buffer = '';
			if (err instanceof Error) {
				console.log(`${err}\n${err.stack}`);
			} else {
				console.log(`Error: ${err}`);
			}
		}
		showPrompt();
	} else if (key === 'Backspace') {
		if (buffer.length) {
			buffer = buffer.slice(0, -1);
			Switch.print(`${cursor.backward(2)}${erase.lineEnd}${cursorChar}`);
		}
	} else if (key === 'Escape') {
		Switch.exit();
	}
});

showPrompt();
