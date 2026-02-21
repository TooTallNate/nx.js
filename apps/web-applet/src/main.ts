// Web Applet Demo — opens the Switch's built-in browser
//
// The browser opens non-blocking, so the nx.js event loop keeps running.
// When jsExtension is enabled, the browser and nx.js can communicate
// via window.nx.sendMessage() / window.nx.onMessage.

const applet = new Switch.WebApplet('https://nxjs.n8.io');

applet.addEventListener('exit', () => {
	console.log('Browser closed');
	Switch.exit();
});

console.log('Opening browser...');
await applet.start();
console.log('Browser opened!');
