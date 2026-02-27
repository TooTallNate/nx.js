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

test.run();
