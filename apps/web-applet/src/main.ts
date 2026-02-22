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
	console.log('Received message from browser:', e.data);

	let msg: any;
	try {
		msg = JSON.parse(e.data);
	} catch (parseErr: any) {
		console.error('Failed to parse message:', parseErr.message);
		return;
	}

	console.log(`RPC: type=${msg.type}, id=${msg.id}, data=${JSON.stringify(msg.data)}`);
	let response: any;

	try {
		switch (msg.type) {
			case 'ls': {
				console.log(`ls: ${msg.data.path}`);
				const entries = Switch.readDirSync(msg.data.path);
				console.log(`ls: got ${entries.length} entries`);
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
				console.log(`read: ${msg.data.path}`);
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
		console.error(`RPC error for ${msg.type}:`, err.message || String(err));
		response = { error: err.message || String(err) };
	}

	const reply = JSON.stringify({ id: msg.id, data: response });
	console.log(`Sending reply for id=${msg.id}, length=${reply.length}`);
	const sent = applet.sendMessage(reply);
	console.log(`sendMessage result: ${sent}`);
});

applet.addEventListener('exit', () => {
	console.log('File browser closed');
});

console.log('Starting offline file browser...');
await applet.start();
console.log(`File browser started in ${applet.mode} mode`);
