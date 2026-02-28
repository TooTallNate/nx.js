import { WebSocketServer } from '@nx.js/ws';

const wss = new WebSocketServer({ port: 8080 });

wss.addEventListener('listening', () => {
	console.log('WebSocket server is listening on port 8080');
});

wss.addEventListener('connection', (e) => {
	const { socket, request } = e.detail;
	console.log('Client connected from', request.url);

	socket.addEventListener('message', (ev) => {
		console.log('Received:', ev.data);
		socket.send(`Echo: ${ev.data}`);
	});

	socket.addEventListener('close', (ev) => {
		console.log('Client disconnected', ev.code, ev.reason);
	});
});
