import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('WebSocket');

test('WebSocket global', () => {
	const desc = Object.getOwnPropertyDescriptor(globalThis, 'WebSocket')!;
	assert.ok(desc);
	assert.equal(desc.writable, true);
	assert.equal(desc.configurable, true);
	assert.equal(
		Object.prototype.toString.call(WebSocket),
		'[object Function]',
	);
});

test('WebSocket.CONNECTING is 0', () => {
	assert.equal(WebSocket.CONNECTING, 0);
});

test('WebSocket.OPEN is 1', () => {
	assert.equal(WebSocket.OPEN, 1);
});

test('WebSocket.CLOSING is 2', () => {
	assert.equal(WebSocket.CLOSING, 2);
});

test('WebSocket.CLOSED is 3', () => {
	assert.equal(WebSocket.CLOSED, 3);
});

test('constructor throws on invalid URL scheme', () => {
	let err: DOMException | undefined;
	try {
		new WebSocket('http://example.com');
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err!.name, 'SyntaxError');
});

test('constructor throws on duplicate protocols', () => {
	let err: DOMException | undefined;
	try {
		new WebSocket('ws://example.com', ['foo', 'foo']);
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err!.name, 'SyntaxError');
});

test('readyState starts at CONNECTING', () => {
	const ws = new WebSocket('ws://example.com');
	assert.equal(ws.readyState, WebSocket.CONNECTING);
	ws.close();
});

test('url property strips fragment', () => {
	const ws = new WebSocket('ws://example.com/path#frag');
	assert.equal(ws.url, 'ws://example.com/path');
	ws.close();
});

test('binaryType defaults to blob', () => {
	const ws = new WebSocket('ws://example.com');
	assert.equal(ws.binaryType, 'blob');
	ws.close();
});

test('binaryType can be set to arraybuffer', () => {
	const ws = new WebSocket('ws://example.com');
	ws.binaryType = 'arraybuffer';
	assert.equal(ws.binaryType, 'arraybuffer');
	ws.close();
});

test('close() throws on invalid code', () => {
	const ws = new WebSocket('ws://example.com');
	let err: DOMException | undefined;
	try {
		ws.close(999);
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err!.name, 'InvalidAccessError');
	ws.close();
});

test('close() throws on reason too long', () => {
	const ws = new WebSocket('ws://example.com');
	let err: DOMException | undefined;
	try {
		ws.close(1000, 'x'.repeat(200));
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err!.name, 'SyntaxError');
	ws.close();
});

test('send() throws in CONNECTING state', () => {
	const ws = new WebSocket('ws://example.com');
	let err: DOMException | undefined;
	try {
		ws.send('hello');
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err!.name, 'InvalidStateError');
	ws.close();
});

test('instance has static constants', () => {
	const ws = new WebSocket('ws://example.com');
	assert.equal(ws.CONNECTING, 0);
	assert.equal(ws.OPEN, 1);
	assert.equal(ws.CLOSING, 2);
	assert.equal(ws.CLOSED, 3);
	ws.close();
});

test('[Symbol.toStringTag] is WebSocket', () => {
	assert.equal(
		Object.prototype.toString.call(new WebSocket('ws://example.com')),
		'[object WebSocket]',
	);
});

// Helper to wait for a WebSocket event with a timeout
function waitForEvent<K extends keyof WebSocketEventMap>(
	ws: WebSocket,
	event: K,
	timeoutMs = 10000,
): Promise<WebSocketEventMap[K]> {
	return new Promise((resolve, reject) => {
		const timer = setTimeout(() => {
			reject(new Error(`Timed out waiting for "${event}" event`));
		}, timeoutMs);
		ws.addEventListener(
			event,
			(ev) => {
				clearTimeout(timer);
				resolve(ev);
			},
			{ once: true },
		);
	});
}

// Helper to open a WebSocket and wait for the connection
function openAndWait(url: string): Promise<WebSocket> {
	return new Promise((resolve, reject) => {
		const ws = new WebSocket(url);
		const timer = setTimeout(() => {
			reject(new Error(`Timed out connecting to ${url}`));
		}, 10000);
		ws.addEventListener(
			'open',
			() => {
				clearTimeout(timer);
				resolve(ws);
			},
			{ once: true },
		);
		ws.addEventListener(
			'error',
			(ev) => {
				clearTimeout(timer);
				reject(new Error(`WebSocket error connecting to ${url}`));
			},
			{ once: true },
		);
	});
}

// --- Live echo server tests (wss://) ---

test('wss: connect and echo text message', async () => {
	const ws = await openAndWait('wss://echo.websocket.org');
	assert.equal(ws.readyState, WebSocket.OPEN);

	const echoPromise = waitForEvent(ws, 'message');
	ws.send('hello wss');
	const ev = await echoPromise;
	assert.equal(ev.data, 'hello wss');

	ws.close();
	await waitForEvent(ws, 'close');
	assert.equal(ws.readyState, WebSocket.CLOSED);
});

test('wss: echo binary (ArrayBuffer) message', async () => {
	const ws = await openAndWait('wss://echo.websocket.org');
	ws.binaryType = 'arraybuffer';

	const payload = new Uint8Array([1, 2, 3, 4, 5]);
	const echoPromise = waitForEvent(ws, 'message');
	ws.send(payload.buffer);
	const ev = await echoPromise;
	assert.ok(ev.data instanceof ArrayBuffer);
	const received = new Uint8Array(ev.data as ArrayBuffer);
	assert.equal(received.length, 5);
	assert.equal(received[0], 1);
	assert.equal(received[4], 5);

	ws.close();
	await waitForEvent(ws, 'close');
});

test('wss: echo multiple messages in order', async () => {
	const ws = await openAndWait('wss://echo.websocket.org');
	const messages = ['first', 'second', 'third'];
	const received: string[] = [];

	const allReceived = new Promise<void>((resolve) => {
		ws.addEventListener('message', (ev) => {
			received.push(ev.data as string);
			if (received.length === messages.length) resolve();
		});
	});

	for (const msg of messages) {
		ws.send(msg);
	}

	await allReceived;
	assert.equal(received, messages);

	ws.close();
	await waitForEvent(ws, 'close');
});

test('wss: close with code and reason', async () => {
	const ws = await openAndWait('wss://echo.websocket.org');

	ws.close(1000, 'normal closure');
	const ev = await waitForEvent(ws, 'close');
	assert.equal(ev.code, 1000);
	assert.equal(ws.readyState, WebSocket.CLOSED);
});

test('wss: onopen/onmessage/onclose handlers work', async () => {
	const ws = new WebSocket('wss://echo.websocket.org');
	const events: string[] = [];

	await new Promise<void>((resolve, reject) => {
		const timer = setTimeout(() => reject(new Error('Timeout')), 10000);
		ws.onopen = () => {
			events.push('open');
			ws.send('handler test');
		};
		ws.onmessage = (ev) => {
			events.push(`message:${ev.data}`);
			ws.close();
		};
		ws.onclose = () => {
			events.push('close');
			clearTimeout(timer);
			resolve();
		};
		ws.onerror = () => {
			clearTimeout(timer);
			reject(new Error('WebSocket error'));
		};
	});

	assert.equal(events, ['open', 'message:handler test', 'close']);
});

// --- Live echo server tests (ws://) ---

test('ws: connect and echo text message', async () => {
	const ws = await openAndWait('ws://echo.websocket.org');
	assert.equal(ws.readyState, WebSocket.OPEN);

	const echoPromise = waitForEvent(ws, 'message');
	ws.send('hello ws');
	const ev = await echoPromise;
	assert.equal(ev.data, 'hello ws');

	ws.close();
	await waitForEvent(ws, 'close');
	assert.equal(ws.readyState, WebSocket.CLOSED);
});

test('ws: echo binary (ArrayBuffer) message', async () => {
	const ws = await openAndWait('ws://echo.websocket.org');
	ws.binaryType = 'arraybuffer';

	const payload = new Uint8Array([10, 20, 30]);
	const echoPromise = waitForEvent(ws, 'message');
	ws.send(payload.buffer);
	const ev = await echoPromise;
	assert.ok(ev.data instanceof ArrayBuffer);
	const received = new Uint8Array(ev.data as ArrayBuffer);
	assert.equal(received.length, 3);
	assert.equal(received[0], 10);
	assert.equal(received[2], 30);

	ws.close();
	await waitForEvent(ws, 'close');
});

test('ws: close with code and reason', async () => {
	const ws = await openAndWait('ws://echo.websocket.org');

	ws.close(1000, 'done');
	const ev = await waitForEvent(ws, 'close');
	assert.equal(ev.code, 1000);
	assert.equal(ws.readyState, WebSocket.CLOSED);
});

test.run();
