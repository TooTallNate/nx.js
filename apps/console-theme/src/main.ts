// Configure the on-screen `console` with a Solarized Dark theme.
//
// `console.options` accepts the same options as `new Console()` — theme colors
// (background / foreground / cursor + the full ANSI palette), `fontSize`,
// `lineHeight`, `scrollback`, `cursorStyle` ('block' | 'underline' | 'bar') and
// `cursorOpacity`. For the global `console`, set this BEFORE the first log: the
// underlying terminal is created lazily, so the options apply on first render.
console.options = {
	fontSize: 22,
	cursorStyle: 'bar',
	theme: {
		background: '#002b36', // base03
		foreground: '#839496', // base0
		cursor: '#93a1a1', // base1
		// Normal ANSI colors.
		black: '#073642', // base02
		red: '#dc322f',
		green: '#859900',
		yellow: '#b58900',
		blue: '#268bd2',
		magenta: '#d33682',
		cyan: '#2aa198',
		white: '#eee8d5', // base2
		// Bright ANSI colors.
		brightBlack: '#586e75', // base01
		brightRed: '#cb4b16', // orange
		brightGreen: '#586e75', // base01
		brightYellow: '#657b83', // base00
		brightBlue: '#839496', // base0
		brightMagenta: '#6c71c4', // violet
		brightCyan: '#93a1a1', // base1
		brightWhite: '#fdf6e3', // base3
	},
};

// Wrap text in an SGR background color (40-47 normal, 100-107 bright).
const bgSwatch = (n: number) => `\x1b[${n}m    \x1b[0m`;

console.log('nx.js console theming');
console.log(' - <3 nx.js');
console.log('');

// Show off the Solarized palette via ANSI background SGR codes.
console.log('Solarized Dark palette:');
let row = '';
for (let i = 0; i < 8; i++) row += bgSwatch(40 + i);
console.log('  normal: ' + row);
row = '';
for (let i = 0; i < 8; i++) row += bgSwatch(100 + i);
console.log('  bright: ' + row);
console.log('');

console.log('Current versions:');
console.log(Switch.version);
console.log('');
console.log('Press the + button to exit...');
