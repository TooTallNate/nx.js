import { test } from 'uvu';
import * as assert from 'uvu/assert';

const ctx = Switch.screen.getContext('2d');

test('`Switch.entrypoint` is a string', () => {
	assert.type(Switch.entrypoint, 'string');
	assert.ok(Switch.entrypoint.length > 0);
});

test('`Switch.cwd()` is a URL string representation', async () => {
	const cwd = Switch.cwd();
	assert.type(cwd, 'string');
	assert.ok(cwd.startsWith('sdmc:/'));
	assert.ok(cwd.endsWith('/'));
});

test('`Switch.readDirSync()` works on relative path', () => {
	const files = Switch.readDirSync('.');
	assert.ok(Array.isArray(files));
	assert.ok(files.length > 0);
});

test('`Switch.readDirSync()` works on `sdmc:/` path', () => {
	const files = Switch.readDirSync('sdmc:/');
	assert.ok(Array.isArray(files));
	assert.ok(files.length > 0);
});

test('`Switch.readDirSync()` works on `romfs:/` path', () => {
	const files = Switch.readDirSync('romfs:/');
	assert.ok(Array.isArray(files));
	assert.ok(files.length > 0);
});

test('`Switch.readDirSync()` throws error when directory does not exist', () => {
	let err: Error | undefined;
	try {
		Switch.readDirSync('romfs:/__does_not_exist__');
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
});

test('`Switch.resolveDns()` works', async () => {
	const result = await Switch.resolveDns('n8.io');
	assert.ok(Array.isArray(result));
	assert.ok(result.length > 0);
});

test('`Switch.resolveDns()` rejects with invalid hostname', async () => {
	let err: Error | undefined;
	try {
		await Switch.resolveDns('example-that-definitely-does-not-exist.nxjs');
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
});

test('`Switch.readFile()` works with string path', async () => {
	const data = await Switch.readFile('romfs:/runtime.js');
	assert.ok(data instanceof ArrayBuffer);
	assert.ok(data.byteLength > 0);
});

test('`Switch.readFile()` works with URL path', async () => {
	const data = await Switch.readFile(new URL(Switch.entrypoint));
	assert.ok(data instanceof ArrayBuffer);
	assert.ok(data.byteLength > 0);
});

test('`Switch.readFile()` rejects when file does not exist', async () => {
	let err: Error | undefined;
	try {
		await Switch.readFile('romfs:/__does_not_exist__');
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
});

test('`Switch.readFile()` rejects when attempting to read a directory', async () => {
	let err: Error | undefined;
	try {
		await Switch.readFile('.');
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
});

test('`Switch.stat()` returns file information', async () => {
	const stat = await Switch.stat(Switch.entrypoint);
	assert.ok(stat.size > 0);
});

test('`Switch.stat()` rejects when file does not exist', async () => {
	let err: Error | undefined;
	try {
		await Switch.stat('romfs:/__does_not_exist__');
	} catch (_err) {
		err = _err as Error;
	}
	assert.ok(err);
});

test('`CanvasContext2D#getImageData()`', () => {
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 1, 1);
	const data = ctx.getImageData(0, 0, 1, 1);
	assert.equal(data.data[0], 255);
	assert.equal(data.data[1], 0);
	assert.equal(data.data[2], 0);
	assert.equal(data.data[3], 255);
});

test('FormData', async () => {
	const file = new File(['conte', new Blob(['nts'])], 'file.txt', {
		type: 'text/plain',
	});
	assert.equal(file.name, 'file.txt');
	assert.equal(file.type, 'text/plain');

	const form = new FormData();
	form.append('file', file);
	form.append('string', 'string-value');

	const r = new Response(form);
	const form2 = await r.formData();
	assert.ok(form2 instanceof FormData);
	assert.ok(form2.get('string') === 'string-value');
	assert.ok(form2.get('missing') === null);

	const file2 = form2.get('file');
	assert.ok(file2 instanceof File);
	assert.ok(file2.name === 'file.txt');
	assert.ok(file2.type === 'text/plain');
	assert.ok((await file2.text()) === 'contents');
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

test.run();
