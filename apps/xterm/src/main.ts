import { Hid } from 'nxjs-constants';
import { Terminal, type IBuffer } from 'xterm-headless';

const { Button } = Hid;

// Register "Geist Mono" font
const fontUrl = new URL('fonts/GeistMono-Regular.otf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Geist Mono', fontData);
Switch.fonts.add(font);

const fontSize = 24;
const charWidth = fontSize * (2 / 3);
const lineHeight = fontSize;
const ctx = Switch.screen.getContext('2d');

const term = new Terminal({
	cols: 80,
	rows: 30,
	scrollback: 500,
	allowProposedApi: true,
});

const cell = term.buffer.normal.getNullCell();

const colors = [
	'black',
	'red',
	'green',
	'yellow',
	'blue',
	'magenta',
	'cyan',
	'white',
];

function render(buff: IBuffer) {
	ctx.fillStyle = 'black';
	ctx.fillRect(0, 0, Switch.screen.width, Switch.screen.height);
	for (let y = 0; y < term.rows; y++) {
		const line = buff.getLine(buff.viewportY + y);
		if (line) {
			for (let x = 0; x < line.length; x++) {
				line.getCell(x, cell);
				const val = cell.getChars();
				const fgColor = colors[cell.getFgColor()];
				ctx.font = `${fontSize}px "Geist Mono"`;
				ctx.fillStyle = fgColor ?? 'white';
				ctx.fillText(val, x * charWidth, (y + 1) * lineHeight);
			}
		}
	}

	// Draw cursor
	ctx.fillStyle = 'white';
	const c = buff.length - term.rows + buff.viewportY + buff.cursorY;
	ctx.fillRect(
		buff.cursorX * charWidth,
		c * lineHeight,
		charWidth,
		lineHeight
	);

	ctx.fillText(
		JSON.stringify({
			r: term.rows,
			l: buff.length,
			v: buff.viewportY,
			y: buff.cursorY,
			c,
		}),
		600,
		600
	);
}

term.buffer.onBufferChange(render);

//term.write("I'm just the default text :(\r\n");
for (let i = 0; i < 30; i++) {
	term.write(`${i}\r\n`);
}
term.write("\u001b[31mBut now I'm red!\r\n");
term.write('\u001b[34mOr am I blue?\r\n');
term.write("\u001b[32mHa, I'm green, obviously.\r\n");
term.write(
	'\u001b[0m\u001b[Hhello world 😁 this is a really long text that definitely should wrap around to the next line waaaaaaaaaaaaaaaaaaaaaaaaaaaaatttttttttttt',
	() => {
		render(term.buffer.active);
	}
);

Switch.addEventListener('buttondown', (e) => {
	if (e.detail & Button.AnyUp) {
		term.scrollLines(-1);
		render(term.buffer.active);
	} else if (e.detail & Button.AnyDown) {
		term.scrollLines(1);
		render(term.buffer.active);
	} else if (e.detail & Button.L) {
		term.scrollToTop();
		render(term.buffer.active);
	} else if (e.detail & Button.R) {
		term.scrollToBottom();
		render(term.buffer.active);
	}
});
