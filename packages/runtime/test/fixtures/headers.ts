import { test } from '../src/tap';

test('Headers - basic get/set/has/delete', (t) => {
	const h = new Headers();
	h.set('Content-Type', 'text/plain');
	t.equal(h.get('content-type'), 'text/plain', 'get is case-insensitive');
	t.equal(h.has('Content-Type'), true, 'has returns true');
	h.delete('content-type');
	t.equal(h.has('content-type'), false, 'has returns false after delete');
	t.equal(h.get('content-type'), null, 'get returns null after delete');
});

test('Headers - append combines values', (t) => {
	const h = new Headers();
	h.append('Accept', 'text/html');
	h.append('Accept', 'application/json');
	t.equal(
		h.get('accept'),
		'text/html, application/json',
		'append combines with comma',
	);
});

test('Headers - iteration is sorted by name', (t) => {
	const h = new Headers();
	h.set('Zebra', '1');
	h.set('Alpha', '2');
	h.set('Middle', '3');
	const keys: string[] = [];
	for (const [name] of h) {
		keys.push(name);
	}
	t.equal(keys[0], 'alpha', 'first key is alpha');
	t.equal(keys[1], 'middle', 'second key is middle');
	t.equal(keys[2], 'zebra', 'third key is zebra');
});

test('Headers - keys() is sorted', (t) => {
	const h = new Headers();
	h.set('Z', '1');
	h.set('A', '2');
	const keys = [...h.keys()];
	t.equal(keys[0], 'a', 'first key');
	t.equal(keys[1], 'z', 'second key');
});

test('Headers - values() is sorted by key', (t) => {
	const h = new Headers();
	h.set('Z', 'last');
	h.set('A', 'first');
	const vals = [...h.values()];
	t.equal(vals[0], 'first', 'first value (from key a)');
	t.equal(vals[1], 'last', 'second value (from key z)');
});

test('Headers - entries() is sorted', (t) => {
	const h = new Headers();
	h.set('B', '2');
	h.set('A', '1');
	const entries = [...h.entries()];
	t.equal(entries[0][0], 'a', 'first entry key');
	t.equal(entries[1][0], 'b', 'second entry key');
});

test('Headers - forEach is sorted', (t) => {
	const h = new Headers();
	h.set('C', '3');
	h.set('A', '1');
	const keys: string[] = [];
	h.forEach((_v, k) => keys.push(k));
	t.equal(keys[0], 'a', 'forEach first key');
	t.equal(keys[1], 'c', 'forEach second key');
});

test('Headers - getSetCookie returns a copy', (t) => {
	const h = new Headers();
	h.append('Set-Cookie', 'a=1');
	h.append('Set-Cookie', 'b=2');
	const cookies = h.getSetCookie();
	t.equal(cookies.length, 2, 'two cookies');
	cookies.push('c=3');
	const cookies2 = h.getSetCookie();
	t.equal(cookies2.length, 2, 'internal state unchanged after mutation');
});

test('Headers - multiple set-cookie in iteration', (t) => {
	const h = new Headers();
	h.append('Set-Cookie', 'a=1');
	h.append('Set-Cookie', 'b=2');
	h.set('Content-Type', 'text/html');
	const entries = [...h.entries()];
	const cookieEntries = entries.filter(([k]) => k === 'set-cookie');
	t.equal(cookieEntries.length, 2, 'set-cookie yields separate entries');
	t.equal(cookieEntries[0][1], 'a=1', 'first set-cookie value');
	t.equal(cookieEntries[1][1], 'b=2', 'second set-cookie value');
});

test('Headers - value normalization strips whitespace', (t) => {
	const h = new Headers();
	h.set('X-Test', '  hello  ');
	t.equal(h.get('x-test'), 'hello', 'leading/trailing spaces stripped');
	h.set('X-Tab', '\thello\t');
	t.equal(h.get('x-tab'), 'hello', 'leading/trailing tabs stripped');
	h.set('X-Mixed', ' \t hello \t ');
	t.equal(h.get('x-mixed'), 'hello', 'mixed whitespace stripped');
});

test('Headers - constructor with init pairs', (t) => {
	const h = new Headers([
		['X-Foo', 'bar'],
		['X-Baz', 'qux'],
	]);
	t.equal(h.get('x-foo'), 'bar', 'init pair 1');
	t.equal(h.get('x-baz'), 'qux', 'init pair 2');
});

test('Headers - constructor with init object', (t) => {
	const h = new Headers({ 'X-Foo': 'bar' });
	t.equal(h.get('x-foo'), 'bar', 'init from object');
});

test('Headers - constructor with Headers', (t) => {
	const h1 = new Headers();
	h1.set('X-Foo', 'bar');
	const h2 = new Headers(h1);
	t.equal(h2.get('x-foo'), 'bar', 'init from Headers');
});
