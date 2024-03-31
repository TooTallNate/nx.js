import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('fetch');

test('fetch global', () => {
	const desc = Object.getOwnPropertyDescriptor(globalThis, 'fetch')!;
	assert.equal(desc.writable, true);
	assert.equal(desc.enumerable, true);
	assert.equal(desc.configurable, true);
	assert.equal(Object.prototype.toString.call(fetch), '[object Function]');
});

test('Request - no `body` for `GET`', () => {
	let err: Error | undefined;
	try {
		new Request('http://example.com', { body: 'bad' });
	} catch (_err: any) {
		err = _err;
	}
	assert.ok(err);
	assert.equal(err.name, 'TypeError');
	assert.equal(err.message, 'Body not allowed for GET or HEAD requests');
});

test('Should work with `data:`', async () => {
	const res = await fetch('data:text/plain;base64,SGVsbG8sIFdvcmxkIQ%3D%3D');
	assert.equal(res.ok, true);
	assert.equal(res.status, 200);
	const body = await res.text();
	assert.equal(body, 'Hello, World!');
});

test('Should work with `http:`', async () => {
	const res = await fetch('http://jsonip.com');
	assert.equal(res.url, 'http://jsonip.com/');
	assert.equal(res.ok, true);
	assert.equal(res.status, 200);
	const body = await res.json();
	assert.type(body.ip, 'string');
});

test('Should work with `https:`', async () => {
	const res = await fetch('https://jsonip.com');
	assert.equal(res.url, 'https://jsonip.com/');
	assert.equal(res.ok, true);
	assert.equal(res.status, 200);
	const body = await res.json();
	assert.type(body.ip, 'string');
});

test('Should work with `romfs:`', async () => {
	const res = await fetch('romfs:/runtime.js');
	assert.equal(res.url, 'romfs:/runtime.js');
	assert.equal(res.ok, true);
	assert.equal(res.status, 200);
	const body = await res.text();
	assert.ok(body.endsWith('//# sourceMappingURL=runtime.js.map\n'));
});

test(`Should follow redirects - 301`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/301', {
		method: 'POST',
	});
	assert.equal(res.redirected, true);
	assert.equal(res.status, 200);
	assert.equal(res.url, 'https://dump.n8.io/');
	const json = await res.json();
	assert.equal(json.request.method, 'GET');
});

test(`Should not follow redirects - 301`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/301', {
		redirect: 'manual',
		method: 'POST',
	});
	assert.equal(res.redirected, false);
	assert.equal(res.status, 301);
	assert.equal(res.url, 'https://nxjs.n8.io/tests/redirect/301');
});

test(`Should follow redirects - 302`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/302', {
		method: 'POST',
	});
	assert.equal(res.redirected, true);
	assert.equal(res.status, 200);
	assert.equal(res.url, 'https://dump.n8.io/');
	const json = await res.json();
	assert.equal(json.request.method, 'GET');
});

test(`Should not follow redirects - 302`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/302', {
		redirect: 'manual',
		method: 'POST',
	});
	assert.equal(res.redirected, false);
	assert.equal(res.status, 302);
	assert.equal(res.url, 'https://nxjs.n8.io/tests/redirect/302');
});

test(`Should follow redirects - 307`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/307', {
		method: 'POST',
	});
	assert.equal(res.redirected, true);
	assert.equal(res.status, 200);
	assert.equal(res.url, 'https://dump.n8.io/');
	const json = await res.json();
	assert.equal(json.request.method, 'POST');
});

test(`Should not follow redirects - 307`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/307', {
		redirect: 'manual',
		method: 'POST',
	});
	assert.equal(res.redirected, false);
	assert.equal(res.status, 307);
	assert.equal(res.url, 'https://nxjs.n8.io/tests/redirect/307');
});

test(`Should follow redirects - 308`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/308', {
		method: 'POST',
	});
	assert.equal(res.redirected, true);
	assert.equal(res.status, 200);
	assert.equal(res.url, 'https://dump.n8.io/');
	const json = await res.json();
	assert.equal(json.request.method, 'POST');
});

test(`Should not follow redirects - 308`, async () => {
	const res = await fetch('https://nxjs.n8.io/tests/redirect/308', {
		redirect: 'manual',
		method: 'POST',
	});
	assert.equal(res.redirected, false);
	assert.equal(res.status, 308);
	assert.equal(res.url, 'https://nxjs.n8.io/tests/redirect/308');
});

test.run();
