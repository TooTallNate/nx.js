// Web Applet Demo — opens the nx.js docs in the Switch browser
//
// The docs site detects window.nx and sends messages back to the app.

const applet = new Switch.WebApplet('https://nxjs.n8.io');
applet.jsExtension = true;

applet.addEventListener('message', (e: any) => {
	try {
		const msg = JSON.parse(e.data);
		console.log(`[browser] ${msg.type}: ${msg.title || msg.url || e.data}`);
	} catch {
		console.log(`[browser] ${e.data}`);
	}
});

applet.addEventListener('exit', () => {
	console.log('Browser closed');
	Switch.exit();
});

console.log('Opening nx.js docs in browser...');
await applet.start();
console.log('Browser opened!');
