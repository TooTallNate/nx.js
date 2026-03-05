import { test } from '../src/tap';

// --- Request constructor: credentials ---

test('Request constructor with init overriding credentials', (t) => {
	const original = new Request('https://example.com', {
		method: 'POST',
		credentials: 'omit',
	});
	const derived = new Request(original, { credentials: 'include' });
	t.equal(derived.credentials, 'include', 'init overrides credentials');
	t.equal(original.credentials, 'omit', 'original unchanged');
});

// --- Request constructor: referrer ---

test('Request constructor copies referrer from input Request', (t) => {
	// Use same-origin referrer (Chrome enforces same-origin policy for referrer)
	const origin =
		typeof location !== 'undefined' ? location.origin : 'https://localhost';
	const original = new Request('https://example.com', {
		method: 'POST',
		referrer: origin + '/page',
	});
	const derived = new Request(original);
	t.equal(derived.referrer, origin + '/page', 'referrer copied');
});

test('Request constructor with init overriding referrer', (t) => {
	const origin =
		typeof location !== 'undefined' ? location.origin : 'https://localhost';
	const original = new Request('https://example.com', {
		method: 'POST',
		referrer: origin + '/page',
	});
	const derived = new Request(original, {
		referrer: origin + '/other',
	});
	t.equal(derived.referrer, origin + '/other', 'init overrides referrer');
	t.equal(original.referrer, origin + '/page', 'original referrer unchanged');
});

// --- Request.clone() ---

test('Request.clone() returns independent copy with same properties', (t) => {
	const req = new Request('https://example.com/path', {
		method: 'POST',
		headers: { 'X-Custom': 'value' },
		credentials: 'include',
	});
	const cloned = req.clone();
	t.equal(cloned.url, req.url, 'url matches');
	t.equal(cloned.method, 'POST', 'method matches');
	t.equal(cloned.credentials, 'include', 'credentials matches');
	t.equal(cloned.headers.get('x-custom'), 'value', 'headers copied');

	// Modifying clone headers should not affect original
	cloned.headers.set('x-custom', 'modified');
	t.equal(req.headers.get('x-custom'), 'value', 'original headers unchanged');
});

test('Request.clone() with body — both readable', async (t) => {
	const req = new Request('https://example.com', {
		method: 'POST',
		body: 'hello world',
	});
	const cloned = req.clone();

	const text1 = await req.text();
	const text2 = await cloned.text();
	t.equal(text1, 'hello world', 'original body');
	t.equal(text2, 'hello world', 'cloned body');
});

test('Request.clone() throws if body already used', async (t) => {
	const req = new Request('https://example.com', {
		method: 'POST',
		body: 'data',
	});
	await req.text(); // consume
	let threw = false;
	try {
		req.clone();
	} catch (e) {
		threw = true;
	}
	t.equal(threw, true, 'throws on bodyUsed');
});

// --- Request.signal ---

test('Request without signal has a signal property', (t) => {
	const req = new Request('https://example.com');
	t.notEqual(req.signal, null, 'signal is not null');
	t.notEqual(req.signal, undefined, 'signal is not undefined');
	t.equal(req.signal.aborted, false, 'signal is not aborted');
});

test('Request signal from init is preserved', (t) => {
	const controller = new AbortController();
	const req = new Request('https://example.com', {
		signal: controller.signal,
	});
	t.equal(req.signal.aborted, false, 'not aborted initially');
	controller.abort();
	t.equal(req.signal.aborted, true, 'aborted after controller.abort()');
});
