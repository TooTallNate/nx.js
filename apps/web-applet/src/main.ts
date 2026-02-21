// Web Applet Demo — opens the Switch browser to a URL
//
// Usage:
//   Mode 1 (online): Opens a web URL, blocks until user closes browser
//   Mode 2 (offline): Opens HTML from romfs (place index.html in romfs/)

// --- Mode 1: Open a URL (blocking) ---

const applet = new Switch.WebApplet('https://nxjs.n8.io');

console.log('Opening web browser...');
const result = await applet.show();

console.log('Browser closed');
console.log('Exit reason:', result.exitReason);
if (result.lastUrl) {
	console.log('Last URL:', result.lastUrl);
}
