import { cursor } from 'sisteransi';
import util from 'node:util';
import { Writable } from 'node:stream';
import { emitKeypressEvents } from 'node:readline';
import { REPL } from './src/index';

const encoder = new TextEncoder();
const writable = Writable.toWeb(process.stdout);
const repl = new REPL(writable.getWriter(), {
	inspect(v: unknown): string {
		return util.inspect(v, { colors: true });
	},
});
repl.renderPrompt();

emitKeypressEvents(process.stdin);
process.stdin.on('keypress', (str, key) => {
	if (key.ctrl && key.name === 'c') {
		process.stdout.write(cursor.show);
		process.exit();
	} else if (key.name === 'return') {
		repl.submit();
	} else if (key.name === 'backspace') {
		repl.backspace();
	} else if (key.name === 'left') {
		repl.arrowLeft();
	} else if (key.name === 'right') {
		repl.arrowRight();
	} else if (key.name === 'up') {
		repl.arrowUp();
	} else if (key.name === 'down') {
		repl.arrowDown();
	} else if (key.name === 'escape') {
		process.stdout.write(cursor.show);
		process.exit();
	} else {
		repl.write(encoder.encode(str));
	}
});
process.stdin.setRawMode(true);
process.stdin.resume();
process.stdout.write(cursor.hide);
