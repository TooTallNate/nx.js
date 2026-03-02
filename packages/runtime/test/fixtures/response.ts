import { test } from '../src/tap';

// --- Response constructor ---

test('Response constructor defaults', (t) => {
	const res = new Response();
	t.equal(res.status, 200, 'default status is 200');
	t.equal(res.statusText, '', 'default statusText is empty');
	t.equal(res.ok, true, 'ok is true for 200');
	t.equal(res.type, 'default', 'default type');
	t.equal(res.url, '', 'default url is empty');
	t.equal(res.redirected, false, 'default redirected is false');
	t.equal(res.body, null, 'default body is null');
	t.equal(res.bodyUsed, false, 'default bodyUsed is false');
});

test('Response with status and statusText', (t) => {
	const res = new Response(null, { status: 404, statusText: 'Not Found' });
	t.equal(res.status, 404, 'status');
	t.equal(res.statusText, 'Not Found', 'statusText');
	t.equal(res.ok, false, 'ok is false for 404');
});

test('Response with body string', async (t) => {
	const res = new Response('hello');
	t.notEqual(res.body, null, 'body is not null');
	t.equal(res.bodyUsed, false, 'bodyUsed before read');
	const text = await res.text();
	t.equal(text, 'hello', 'body text');
	t.equal(res.bodyUsed, true, 'bodyUsed after read');
});

// --- Response.clone() ---

test('Response.clone() with null body', (t) => {
	const res = new Response(null, { status: 204, statusText: 'No Content' });
	const cloned = res.clone();
	t.equal(cloned.status, 204, 'cloned status');
	t.equal(cloned.statusText, 'No Content', 'cloned statusText');
	t.equal(cloned.body, null, 'cloned body is null');
	t.equal(cloned.bodyUsed, false, 'cloned bodyUsed');
});

test('Response.clone() copies headers', (t) => {
	const res = new Response(null, {
		headers: { 'X-Custom': 'value', 'Content-Type': 'text/plain' },
	});
	const cloned = res.clone();
	t.equal(cloned.headers.get('x-custom'), 'value', 'cloned header');
	t.equal(
		cloned.headers.get('content-type'),
		'text/plain',
		'cloned content-type',
	);

	// Modifying clone headers should not affect original
	cloned.headers.set('x-custom', 'modified');
	t.equal(res.headers.get('x-custom'), 'value', 'original header unchanged');
});

test('Response.clone() tees body stream', async (t) => {
	const res = new Response('hello world');
	const cloned = res.clone();

	const text1 = await res.text();
	const text2 = await cloned.text();
	t.equal(text1, 'hello world', 'original body');
	t.equal(text2, 'hello world', 'cloned body');
});

test('Response.clone() throws on used body', async (t) => {
	const res = new Response('data');
	await res.text(); // consume the body
	t.throws(() => res.clone(), 'already used', 'throws on bodyUsed');
});

test('Response.clone() preserves default properties', (t) => {
	const res = new Response(null, { status: 301 });
	const cloned = res.clone();
	// type, url, redirected are read-only in Chrome (set by fetch internals)
	// so we test that clone preserves the default values
	t.equal(cloned.type, 'default', 'cloned type');
	t.equal(cloned.url, '', 'cloned url');
	t.equal(cloned.redirected, false, 'cloned redirected');
});

// --- Response.redirect() ---

test('Response.redirect() with default status', (t) => {
	const res = Response.redirect('https://example.com/');
	t.equal(res.status, 302, 'default redirect status is 302');
	t.equal(
		res.headers.get('location'),
		'https://example.com/',
		'location header',
	);
});

test('Response.redirect() with valid statuses', (t) => {
	const validStatuses = [301, 302, 303, 307, 308];
	for (const status of validStatuses) {
		const res = Response.redirect('https://example.com/', status);
		t.equal(res.status, status, `status ${status}`);
		t.equal(
			res.headers.get('location'),
			'https://example.com/',
			`location for ${status}`,
		);
	}
});

test('Response.redirect() throws on invalid status', (t) => {
	t.throws(
		() => Response.redirect('https://example.com/', 200),
		'Invalid status',
		'throws on 200',
	);
	t.throws(
		() => Response.redirect('https://example.com/', 404),
		'Invalid status',
		'throws on 404',
	);
	t.throws(
		() => Response.redirect('https://example.com/', 500),
		'Invalid status',
		'throws on 500',
	);
});

test('Response.redirect() converts URL object to string', (t) => {
	const url = new URL('https://example.com/path?q=1');
	const res = Response.redirect(url);
	t.equal(
		res.headers.get('location'),
		'https://example.com/path?q=1',
		'URL object converted',
	);
});

// --- Response.error() ---

test('Response.error() returns error response', (t) => {
	const res = Response.error();
	t.equal(res.status, 0, 'error status is 0');
	t.equal(res.type, 'error', 'error type');
});

// --- Response.json() ---

test('Response.json() creates JSON response', async (t) => {
	const data = { key: 'value', num: 42 };
	const res = Response.json(data);
	t.equal(res.headers.get('content-type'), 'application/json', 'content-type');
	const body = await res.json();
	t.deepEqual(body, data, 'json body');
});
