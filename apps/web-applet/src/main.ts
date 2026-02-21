import { listen } from '@nx.js/http';

// File browser HTML served by the local HTTP server
const html = `<!DOCTYPE html>
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
  #path-bar button { background: #e94560; color: white; border: none; padding: 6px 16px; border-radius: 4px;
    cursor: pointer; font-size: 14px; }
  #listing { padding: 8px 24px; }
  .entry { display: flex; align-items: center; padding: 14px 16px; border-bottom: 1px solid #16213e;
    cursor: pointer; border-radius: 4px; gap: 12px; }
  .entry:hover { background: #16213e; }
  .icon { font-size: 24px; width: 32px; text-align: center; }
  .name { flex: 1; font-size: 16px; word-break: break-all; }
  .size { color: #888; font-size: 14px; font-family: monospace; white-space: nowrap; }
  .loading { text-align: center; padding: 40px; color: #888; font-size: 18px; }
</style>
</head>
<body>
<header><h1>&#128193; nx.js File Browser</h1></header>
<div id="path-bar">
  <button onclick="goUp()">&#11014; Up</button>
  <span id="current-path">sdmc:/</span>
</div>
<div id="listing"><div class="loading">Loading...</div></div>
<script>
var currentPath = 'sdmc:/';

function formatSize(b) {
  if (b < 1024) return b + ' B';
  if (b < 1048576) return (b / 1024).toFixed(1) + ' KB';
  if (b < 1073741824) return (b / 1048576).toFixed(1) + ' MB';
  return (b / 1073741824).toFixed(2) + ' GB';
}

function navigate(path) {
  currentPath = path;
  document.getElementById('current-path').textContent = path;
  document.getElementById('listing').innerHTML = '<div class="loading">Loading...</div>';
  fetch('/api/ls?path=' + encodeURIComponent(path))
    .then(function(r) { return r.json(); })
    .then(function(entries) {
      entries.sort(function(a, b) {
        if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
        return a.name.localeCompare(b.name);
      });
      var html = '';
      for (var i = 0; i < entries.length; i++) {
        var e = entries[i];
        var fullPath = currentPath + (currentPath.endsWith('/') ? '' : '/') + e.name;
        var escaped = fullPath.replace(/'/g, "\\\\'");
        html += '<div class="entry" onclick="navigate(\\'' + escaped + '\\')">';
        html += '<span class="icon">' + (e.isDir ? '&#128193;' : '&#128196;') + '</span>';
        html += '<span class="name">' + e.name + '</span>';
        html += '<span class="size">' + (e.isDir ? '' : formatSize(e.size)) + '</span>';
        html += '</div>';
      }
      if (!entries.length) html = '<div class="loading">Empty directory</div>';
      document.getElementById('listing').innerHTML = html;
    })
    .catch(function(err) {
      document.getElementById('listing').innerHTML = '<div class="loading">Error: ' + err.message + '</div>';
    });
}

function goUp() {
  var parts = currentPath.replace(/\\/$/, '').split('/');
  if (parts.length > 1) { parts.pop(); var p = parts.join('/'); if (!p.endsWith('/')) p += '/'; navigate(p); }
}

navigate('sdmc:/');
</script>
</body>
</html>`;

// Start HTTP server
const { ip } = Switch.networkInfo();
const port = 8080;

listen({
	port,
	fetch(req) {
		const url = new URL(req.url);

		if (url.pathname === '/api/ls') {
			const path = url.searchParams.get('path') || 'sdmc:/';
			try {
				const entries = Switch.readDirSync(path).map((name: string) => {
					const fullPath = path + (path.endsWith('/') ? '' : '/') + name;
					const stat = Switch.statSync(fullPath);
					return { name, isDir: stat ? stat.isDirectory() : false, size: stat ? stat.size : 0 };
				});
				return new Response(JSON.stringify(entries), {
					headers: { 'Content-Type': 'application/json' },
				});
			} catch (err: any) {
				return new Response(JSON.stringify({ error: err.message }), {
					status: 500,
					headers: { 'Content-Type': 'application/json' },
				});
			}
		}

		return new Response(html, {
			headers: { 'Content-Type': 'text/html; charset=utf-8' },
		});
	},
});

console.log(`HTTP server listening on http://${ip}:${port}`);

// Open browser via WifiWebAuthApplet (HTTP = localhost access)
const applet = new Switch.WebApplet(`http://${ip}:${port}`);

applet.addEventListener('exit', () => {
	console.log('Browser closed');
	Switch.exit();
});

console.log(`Starting browser (mode will be: wifi-auth for HTTP)...`);
await applet.start();
console.log(`Browser started in ${applet.mode} mode`);
