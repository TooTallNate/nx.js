import { test } from '../src/tap';

test('URL constructor', (t) => {
	const url = new URL('https://example.com:8080/path?key=value#hash');
	t.equal(url.protocol, 'https:', 'protocol');
	t.equal(url.hostname, 'example.com', 'hostname');
	t.equal(url.port, '8080', 'port');
	t.equal(url.pathname, '/path', 'pathname');
	t.equal(url.search, '?key=value', 'search');
	t.equal(url.hash, '#hash', 'hash');
	t.equal(url.host, 'example.com:8080', 'host');
	t.equal(url.origin, 'https://example.com:8080', 'origin');
});

test('URL with base', (t) => {
	const url = new URL('/relative', 'https://example.com');
	t.equal(url.href, 'https://example.com/relative', 'href');
	t.equal(url.pathname, '/relative', 'pathname');
});

test('URL href setter', (t) => {
	const url = new URL('https://example.com');
	url.href = 'https://other.com/new';
	t.equal(url.hostname, 'other.com', 'hostname after href set');
	t.equal(url.pathname, '/new', 'pathname after href set');
});

test('URL invalid throws', (t) => {
	t.throws(() => new URL('not a url'), undefined, 'invalid URL throws');
});

test('URL.canParse', (t) => {
	t.equal(URL.canParse('https://example.com'), true, 'valid URL');
	t.equal(URL.canParse('not a url'), false, 'invalid URL');
	t.equal(
		URL.canParse('/path', 'https://example.com'),
		true,
		'valid with base',
	);
});

test('URLSearchParams basics', (t) => {
	const params = new URLSearchParams('a=1&b=2&c=3');
	t.equal(params.get('a'), '1', 'get a');
	t.equal(params.get('b'), '2', 'get b');
	t.equal(params.get('c'), '3', 'get c');
	t.equal(params.get('d'), null, 'get missing returns null');
	t.equal(params.has('a'), true, 'has a');
	t.equal(params.has('d'), false, 'has d');
	t.equal(params.size, 3, 'size');
});

test('URLSearchParams append/delete', (t) => {
	const params = new URLSearchParams();
	params.append('key', 'val1');
	params.append('key', 'val2');
	t.equal(params.getAll('key').length, 2, 'getAll after append');
	t.equal(params.get('key'), 'val1', 'get returns first');
	params.delete('key');
	t.equal(params.has('key'), false, 'has after delete');
});

test('URLSearchParams set', (t) => {
	const params = new URLSearchParams('a=1&a=2&a=3');
	params.set('a', 'only');
	t.equal(params.getAll('a').length, 1, 'set replaces all');
	t.equal(params.get('a'), 'only', 'set value');
});

test('URLSearchParams sort', (t) => {
	const params = new URLSearchParams('c=3&a=1&b=2');
	params.sort();
	t.equal(params.toString(), 'a=1&b=2&c=3', 'sorted');
});

test('URLSearchParams iteration', (t) => {
	const params = new URLSearchParams('a=1&b=2');
	const keys: string[] = [];
	const values: string[] = [];
	const entries: [string, string][] = [];

	for (const key of params.keys()) keys.push(key);
	for (const val of params.values()) values.push(val);
	for (const entry of params.entries()) entries.push(entry);

	t.deepEqual(keys, ['a', 'b'], 'keys()');
	t.deepEqual(values, ['1', '2'], 'values()');
	t.deepEqual(
		entries,
		[
			['a', '1'],
			['b', '2'],
		],
		'entries()',
	);
});

test('URL searchParams are linked', (t) => {
	const url = new URL('https://example.com?a=1');
	url.searchParams.set('b', '2');
	t.ok(url.href.includes('b=2'), 'href reflects searchParams change');
	t.ok(url.search.includes('b=2'), 'search reflects searchParams change');
});
