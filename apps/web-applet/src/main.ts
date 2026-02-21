import { listen } from '@nx.js/http';

// ── HTML/CSS/JS for the file browser UI ──

const clientHtml = `<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>nx.js File Browser</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: -apple-system, system-ui, sans-serif; background: #1a1a2e; color: #eee; }
  header { background: #16213e; padding: 16px 24px; border-bottom: 2px solid #e94560; }
  header h1 { font-size: 24px; color: #e94560; }
  #path-bar { background: #0f3460; padding: 10px 24px; font-family: monospace; font-size: 14px; color: #aaa;
    display: flex; align-items: center; gap: 8px; }
  #path-bar button { background: #e94560; color: white; border: none; padding: 4px 12px; border-radius: 4px;
    cursor: pointer; font-size: 14px; }
  #path-bar button:hover { background: #c73e54; }
  #listing { padding: 8px 24px; }
  .entry { display: flex; align-items: center; padding: 12px 16px; border-bottom: 1px solid #16213e;
    cursor: pointer; border-radius: 4px; gap: 12px; }
  .entry:hover { background: #16213e; }
  .icon { font-size: 24px; width: 32px; text-align: center; flex-shrink: 0; }
  .name { flex: 1; font-size: 16px; }
  .size { color: #888; font-size: 14px; font-family: monospace; }
  .loading { text-align: center; padding: 40px; color: #888; }
  #preview { display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,0.9); z-index: 100; padding: 24px; overflow: auto; }
  #preview-close { position: fixed; top: 16px; right: 24px; background: #e94560; color: white;
    border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer; z-index: 101; font-size: 16px; }
  #preview-content { background: #16213e; padding: 16px; border-radius: 8px; margin-top: 48px;
    font-family: monospace; font-size: 13px; white-space: pre-wrap; word-break: break-all; max-height: 80vh; overflow: auto; }
</style>
</head>
<body>
<header><h1>📂 nx.js File Browser</h1></header>
<div id="path-bar">
  <button onclick="goUp()">⬆ Up</button>
  <span id="current-path">sdmc:/</span>
</div>
<div id="listing"><div class="loading">Loading...</div></div>

<div id="preview">
  <button id="preview-close" onclick="closePreview()">✕ Close</button>
  <pre id="preview-content"></pre>
</div>

<script>
var currentPath = 'sdmc:/';
var msgId = 0;
var pending = {};

function send(type, data) {
  var id = ++msgId;
  return new Promise(function(resolve) {
    pending[id] = resolve;
    window.nx.sendMessage(JSON.stringify({ id: id, type: type, data: data }));
  });
}

window.nx.onMessage = function(raw) {
  var msg = JSON.parse(raw);
  if (msg.id && pending[msg.id]) {
    pending[msg.id](msg.data);
    delete pending[msg.id];
  }
};

function formatSize(bytes) {
  if (bytes < 1024) return bytes + ' B';
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
  if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB';
  return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
}

function navigate(path) {
  currentPath = path;
  document.getElementById('current-path').textContent = path;
  document.getElementById('listing').innerHTML = '<div class="loading">Loading...</div>';

  send('ls', { path: path }).then(function(entries) {
    var html = '';
    // Sort: directories first, then alphabetical
    entries.sort(function(a, b) {
      if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
    for (var i = 0; i < entries.length; i++) {
      var e = entries[i];
      var icon = e.isDir ? '📁' : '📄';
      var size = e.isDir ? '' : formatSize(e.size);
      var fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + e.name;
      html += '<div class="entry" onclick="' + (e.isDir ? "navigate('" + fullPath + "')" : "preview('" + fullPath + "')") + '">';
      html += '<span class="icon">' + icon + '</span>';
      html += '<span class="name">' + e.name + '</span>';
      html += '<span class="size">' + size + '</span>';
      html += '</div>';
    }
    if (entries.length === 0) {
      html = '<div class="loading">Empty directory</div>';
    }
    document.getElementById('listing').innerHTML = html;
  });
}

function goUp() {
  var parts = currentPath.replace(/\\/$/, '').split('/');
  if (parts.length > 1) {
    parts.pop();
    var parent = parts.join('/');
    if (!parent.endsWith('/')) parent += '/';
    navigate(parent);
  }
}

function preview(path) {
  document.getElementById('preview').style.display = 'block';
  document.getElementById('preview-content').textContent = 'Loading...';

  send('read', { path: path, maxSize: 64000 }).then(function(result) {
    if (result.error) {
      document.getElementById('preview-content').textContent = 'Error: ' + result.error;
    } else {
      document.getElementById('preview-content').textContent = result.content;
    }
  });
}

function closePreview() {
  document.getElementById('preview').style.display = 'none';
}

navigate('sdmc:/');
</script>
</body>
</html>`;

// ── Backend: HTTP server + WebApplet message handler ──

listen({
	port: 8080,
	fetch() {
		return new Response(clientHtml, {
			headers: { 'Content-Type': 'text/html; charset=utf-8' },
		});
	},
});

const applet = new Switch.WebApplet('http://127.0.0.1:8080');
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
				response = { error: `Unknown message type: ${msg.type}` };
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

await applet.start();
console.log('File browser started at http://localhost:8080');
