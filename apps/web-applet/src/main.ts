// File Browser — demonstrates romfs offline mode with window.nx messaging
//
// The HTML UI is served from romfs (no network required).
// Communication between the browser and nx.js happens via window.nx IPC.

const applet = new Switch.WebApplet('romfs:/index.html');
applet.jsExtension = true;

// Handle RPC messages from the browser
applet.addEventListener('message', (e: any) => {
	const msg = JSON.parse(e.data);
	let response: any;

	try {
		switch (msg.type) {
			case 'ls': {
				const entries = Switch.readDirSync(msg.data.path);
				response = entries.map((name: string) => {
					const fullPath =
						msg.data.path +
						(msg.data.path.endsWith('/') ? '' : '/') +
						name;
					const stat = Switch.statSync(fullPath);
					return {
						name,
						isDir: stat ? stat.isDirectory() : false,
						size: stat ? stat.size : 0,
					};
				});
				break;
			}

			case 'read': {
				const maxSize = msg.data.maxSize || 64000;
				const stat = Switch.statSync(msg.data.path);
				if (!stat) {
					response = { error: 'File not found' };
				} else if (stat.size > maxSize) {
					const buf = Switch.readFileSync(msg.data.path);
					const slice = buf.slice(0, maxSize);
					response = {
						content:
							new TextDecoder().decode(slice) +
							`\n\n... (truncated, ${stat.size} bytes total)`,
					};
				} else {
					const buf = Switch.readFileSync(msg.data.path);
					response = { content: new TextDecoder().decode(buf) };
				}
				break;
			}

			default:
				response = { error: `Unknown command: ${msg.type}` };
		}
	} catch (err: any) {
		response = { error: err.message || String(err) };
	}

	applet.sendMessage(JSON.stringify({ id: msg.id, data: response }));
});

applet.addEventListener('exit', () => {
	console.log('File browser closed');
	Switch.exit();
});

console.log('Starting file browser...');
await applet.start();
console.log('File browser started!');
