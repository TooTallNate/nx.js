import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('fetch');

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
	assert.equal(res.ok, true);
	assert.equal(res.status, 200);
	const body = await res.json();
	assert.type(body.ip, 'string');
});

test('Should work with `romfs:`', async () => {
	const res = await fetch('romfs:/runtime.js');
	assert.equal(res.ok, true);
	assert.equal(res.status, 200);
	const body = await res.text();
	assert.ok(body.endsWith('//# sourceMappingURL=runtime.js.map\n'));
});

test.run();
