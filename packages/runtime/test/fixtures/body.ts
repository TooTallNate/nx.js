import { test } from '../src/tap';

// --- bodyUsed ---

test('bodyUsed is false initially', (t) => {
	const res = new Response('hello');
	t.equal(res.bodyUsed, false, 'bodyUsed starts false');
});

test('bodyUsed is true after text()', async (t) => {
	const res = new Response('hello');
	await res.text();
	t.equal(res.bodyUsed, true, 'bodyUsed after text()');
});

test('bodyUsed is true after arrayBuffer()', async (t) => {
	const res = new Response('hello');
	await res.arrayBuffer();
	t.equal(res.bodyUsed, true, 'bodyUsed after arrayBuffer()');
});

test('bodyUsed is true after json()', async (t) => {
	const res = new Response('{"a":1}');
	await res.json();
	t.equal(res.bodyUsed, true, 'bodyUsed after json()');
});

test('bodyUsed is true after blob()', async (t) => {
	const res = new Response('hello');
	await res.blob();
	t.equal(res.bodyUsed, true, 'bodyUsed after blob()');
});

// --- Double consumption throws TypeError ---

test('text() throws TypeError on second call', async (t) => {
	const res = new Response('hello');
	await res.text();
	try {
		await res.text();
		t.fail('should have thrown');
	} catch (e: any) {
		t.ok(e instanceof TypeError, 'throws TypeError');
	}
});

test('arrayBuffer() throws TypeError on second call', async (t) => {
	const res = new Response('hello');
	await res.arrayBuffer();
	try {
		await res.arrayBuffer();
		t.fail('should have thrown');
	} catch (e: any) {
		t.ok(e instanceof TypeError, 'throws TypeError');
	}
});

test('json() after text() throws TypeError', async (t) => {
	const res = new Response('{"a":1}');
	await res.text();
	try {
		await res.json();
		t.fail('should have thrown');
	} catch (e: any) {
		t.ok(e instanceof TypeError, 'throws TypeError');
	}
});

test('blob() after arrayBuffer() throws TypeError', async (t) => {
	const res = new Response('hello');
	await res.arrayBuffer();
	try {
		await res.blob();
		t.fail('should have thrown');
	} catch (e: any) {
		t.ok(e instanceof TypeError, 'throws TypeError');
	}
});

// --- bodyUsed reflects stream disturbance ---

test('bodyUsed is true after reading from stream', async (t) => {
	const res = new Response('hello');
	const reader = res.body!.getReader();
	await reader.read();
	t.equal(res.bodyUsed, true, 'bodyUsed is true after reading');
	reader.releaseLock();
});

// --- Request bodyUsed ---

test('Request bodyUsed after text()', async (t) => {
	const req = new Request('http://example.com', { method: 'POST', body: 'data' });
	t.equal(req.bodyUsed, false, 'bodyUsed starts false');
	await req.text();
	t.equal(req.bodyUsed, true, 'bodyUsed after text()');
});

test('Request body transferred on construction', async (t) => {
	const req1 = new Request('http://example.com', { method: 'POST', body: 'data' });
	const req2 = new Request(req1);
	t.equal(req1.bodyUsed, true, 'original request bodyUsed after transfer');
	const text = await req2.text();
	t.equal(text, 'data', 'new request has the body');
});

// --- bufferSourceToArrayBuffer offset ---

test('Response from TypedArray with offset reads correctly', async (t) => {
	const buf = new ArrayBuffer(10);
	const view = new Uint8Array(buf);
	for (let i = 0; i < 10; i++) view[i] = i;
	// Create a subarray with offset
	const sub = new Uint8Array(buf, 3, 4); // bytes [3, 4, 5, 6]
	const res = new Response(sub);
	const ab = await res.arrayBuffer();
	const result = new Uint8Array(ab);
	t.equal(result.length, 4, 'correct length');
	t.equal(result[0], 3, 'first byte');
	t.equal(result[1], 4, 'second byte');
	t.equal(result[2], 5, 'third byte');
	t.equal(result[3], 6, 'fourth byte');
});

test('Response from DataView with offset reads correctly', async (t) => {
	const buf = new ArrayBuffer(10);
	const view = new Uint8Array(buf);
	for (let i = 0; i < 10; i++) view[i] = i + 10;
	const dv = new DataView(buf, 2, 5); // bytes [12, 13, 14, 15, 16]
	const res = new Response(dv);
	const ab = await res.arrayBuffer();
	const result = new Uint8Array(ab);
	t.equal(result.length, 5, 'correct length');
	t.equal(result[0], 12, 'first byte');
	t.equal(result[4], 16, 'last byte');
});

// --- FormData round-trip ---

test('FormData round-trip via multipart', async (t) => {
	const fd = new FormData();
	fd.append('name', 'Clutz');
	fd.append('role', 'bot');
	const res = new Response(fd);
	const contentType = res.headers.get('content-type');
	t.ok(contentType?.startsWith('multipart/form-data;'), 'content-type is multipart');
	// Parse it back
	const parsed = await res.formData();
	t.equal(parsed.get('name'), 'Clutz', 'name field');
	t.equal(parsed.get('role'), 'bot', 'role field');
});

test('FormData round-trip with File', async (t) => {
	const fd = new FormData();
	const file = new File(['hello world'], 'test.txt', { type: 'text/plain' });
	fd.append('doc', file);
	fd.append('label', 'test');
	const res = new Response(fd);
	const parsed = await res.formData();
	t.equal(parsed.get('label'), 'test', 'label field');
	const parsedFile = parsed.get('doc') as File;
	t.ok(parsedFile instanceof File, 'doc is a File');
	t.equal(parsedFile.name, 'test.txt', 'filename preserved');
	const text = await parsedFile.text();
	t.equal(text, 'hello world', 'file content preserved');
});

// --- Null body ---

test('null body arrayBuffer returns empty', async (t) => {
	const res = new Response(null);
	const ab = await res.arrayBuffer();
	t.equal(ab.byteLength, 0, 'empty ArrayBuffer');
	t.equal(res.bodyUsed, true, 'bodyUsed after reading null body');
});

test('null body text returns empty string', async (t) => {
	const res = new Response(null);
	const text = await res.text();
	t.equal(text, '', 'empty string');
});
