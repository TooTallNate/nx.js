// The on-screen console is themed declaratively via `romfs/nxjs.ini` — see the
// `[console]` section there (Solarized Dark). No app code is required to style
// the console; the runtime seeds the global `console` from nxjs.ini at startup.
//
// An app can still override at runtime by assigning `console.options`, e.g.:
//
//   console.options = { fontSize: 28, theme: { background: '#1d2021' } };

// Wrap text in an SGR background color (40-47 normal, 100-107 bright).
const bgSwatch = (n: number) => `\x1b[${n}m    \x1b[0m`;

console.log('nx.js console theming');
console.log(' - <3 nx.js');
console.log('');

// Show off the palette (from nxjs.ini) via ANSI background SGR codes.
console.log('Solarized Dark palette (from nxjs.ini):');
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
