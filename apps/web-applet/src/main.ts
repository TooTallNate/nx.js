import { listen } from '@nx.js/http';

// HTML page served to the browser
const html = `<!DOCTYPE html>
<html>
<head><title>nx.js Web Applet Demo</title></head>
<body>
  <h1>Hello from the Web Applet!</h1>
  <button onclick="sendPing()">Send Ping</button>
  <div id="log"></div>
  <script>
    function log(msg) {
      document.getElementById('log').innerHTML += '<p>' + msg + '</p>';
    }
    window.nx.onMessage = function(msg) {
      log('Received: ' + msg);
    };
    function sendPing() {
      window.nx.sendMessage('ping from browser');
      log('Sent: ping');
    }
  </script>
</body>
</html>`;

listen({
	port: 8080,
	fetch(req) {
		return new Response(html, {
			headers: { 'Content-Type': 'text/html' },
		});
	},
});

const applet = new Switch.WebApplet('http://localhost:8080');
applet.jsExtension = true;

applet.addEventListener('message', (e: any) => {
	console.log('Message from browser:', e.data);
	applet.sendMessage('pong from nx.js!');
});

applet.addEventListener('exit', () => {
	console.log('Web applet closed');
	Switch.exit();
});

await applet.start();
console.log('Web applet started!');
