import { HidNpadButton } from '@nx.js/constants';
import { Terminal, type IBuffer } from '@xterm/headless';

// Register "Geist Mono" font
const fontUrl = new URL('fonts/GeistMono-Regular.otf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Geist Mono', fontData!);
fonts.add(font);

const emojiFontUrl = new URL('fonts/Twemoji.ttf', Switch.entrypoint);
const emojiFontData = Switch.readFileSync(emojiFontUrl);
const emojiFont = new FontFace('Twemoji', emojiFontData!);
fonts.add(emojiFont);

const fontSize = 24;
const charWidth = fontSize * (2 / 3);
const lineHeight = fontSize;
const ctx = screen.getContext('2d');

const term = new Terminal({
	cols: 80,
	rows: 30,
	scrollback: 500,
	tabStopWidth: 4,
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
	ctx.fillRect(0, 0, screen.width, screen.height);
	for (let y = 0; y < term.rows; y++) {
		const line = buff.getLine(buff.viewportY + y);
		if (line) {
			for (let x = 0; x < line.length; x++) {
				line.getCell(x, cell);
				const val = cell.getChars();
				const fgColor = colors[cell.getFgColor()];
				const font = /\p{Emoji_Presentation}/u.test(val) ? 'Twemoji' : 'Geist Mono';
				ctx.font = `${fontSize}px "${font}"`;
				ctx.fillStyle = fgColor ?? 'white';
				ctx.fillText(val, x * charWidth, (y + 1) * lineHeight);
			}
		}
	}

	// Draw cursor
	ctx.fillStyle = 'white';
	const c = buff.length - (buff.viewportY + term.rows) + buff.cursorY;
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

addEventListener('buttondown', (e) => {
	if (e.detail & HidNpadButton.AnyUp) {
		term.scrollLines(-1);
		render(term.buffer.active);
	} else if (e.detail & HidNpadButton.AnyDown) {
		term.scrollLines(1);
		render(term.buffer.active);
	} else if (e.detail & HidNpadButton.L) {
		term.scrollToTop();
		render(term.buffer.active);
	} else if (e.detail & HidNpadButton.R) {
		term.scrollToBottom();
		render(term.buffer.active);
	}
});

console.print = (s) => {
	term.write(s.replace(/\n/g, '\r\n'), () => {
		render(term.buffer.active);
	});
};

console.log('hello world');
console.log('hello world');
