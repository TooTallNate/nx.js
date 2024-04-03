import { cursor } from 'sisteransi';
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
	'rgba(7.000000057742, 54.00000058114529, 66.00000366568565, 1)',
	'rgba(220.00000208616257, 50.000000819563866, 47.0000009983778, 1)',
	'rgba(133.00000727176666, 153.00000607967377, 0, 1)',
	'yellow',
	'rgba(38.0000015348196, 139.0000069141388, 210.00000268220901, 1)',
	'magenta',
	'cyan',
	'white',
];

function render(buff: IBuffer) {
	ctx.textAlign = 'start';
	ctx.textBaseline = 'alphabetic';
	ctx.fillStyle = 'rgba(0, 43.00000123679638, 54.00000058114529, 1)';
	ctx.fillRect(0, 0, screen.width, screen.height);

	for (let y = 0; y < term.rows; y++) {
		const line = buff.getLine(buff.viewportY + y);
		if (line) {
			for (let x = 0; x < line.length; x++) {
				line.getCell(x, cell);
				const val = cell.getChars();
				const fgColor = colors[cell.getFgColor()];
				const font = /\p{Emoji_Presentation}/u.test(val)
					? 'Twemoji'
					: 'Geist Mono';
				ctx.font = `${fontSize}px "${font}"`;
				ctx.fillStyle = fgColor ?? 'white';
				ctx.fillText(val, x * charWidth, (y + 1) * lineHeight);
			}
		}
	}

	// Draw cursor
	ctx.fillStyle = 'white';
	const c = buff.length - (buff.viewportY + term.rows) + buff.cursorY;
	ctx.fillRect(buff.cursorX * charWidth, c * lineHeight, charWidth, lineHeight);

	// Debugging
	ctx.font = `14px "Geist Mono"`;
	ctx.textAlign = 'right';
	ctx.textBaseline = 'bottom';
	ctx.fillText(
		JSON.stringify({
			r: term.rows,
			l: buff.length,
			v: buff.viewportY,
			y: buff.cursorY,
			c,
		}),
		screen.width - 10,
		screen.height - 10,
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
	},
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
	} else if (e.detail & HidNpadButton.AnyLeft) {
		console.print(cursor.up());
	} else if (e.detail & HidNpadButton.AnyRight) {
		console.print(cursor.down());
	}
});

console.print = (s) => {
	term.write(s.replace(/\n/g, '\r\n'), () => {
		render(term.buffer.active);
	});
};

console.log('hello world');
console.log('hello world');
console.log(term.options.theme);
