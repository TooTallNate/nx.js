// File Browser — Offline mode with window.nx IPC
//
// HTML is loaded from the app's html-document NCA (no network required).
// Communication happens via window.nx messaging.
//
// To build as NSP: pnpm nsp
// The html-document/ directory is automatically packaged into the NCA.

const applet = new Switch.WebApplet('offline:/.htdocs/index.html');
applet.jsExtension = true;

// Handle RPC messages from the browser
applet.addEventListener('message', (e: any) => {
	let msg: any;
	try {
		msg = JSON.parse(e.data);
	} catch {
		console.error('Failed to parse message from browser');
		return;
	}

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
					try {
						const stat = Switch.statSync(fullPath);
						const isDir = stat
							? (stat.mode & 0o170000) === 0o040000
							: false;
						return {
							name,
							isDir,
							size: stat ? stat.size : 0,
						};
					} catch {
						return {
							name,
							isDir: false,
							size: -1,
						};
					}
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
});

console.log('Starting offline file browser...');
await applet.start();
console.log(`File browser started in ${applet.mode} mode`);
