import { test } from '../src/tap';

test('localStorage setItem/getItem roundtrip', (t) => {
	localStorage.setItem('test-key', 'test-value');
	t.equal(localStorage.getItem('test-key'), 'test-value', 'getItem returns stored value');
	localStorage.clear();
});

test('localStorage removeItem', (t) => {
	localStorage.setItem('remove-me', 'value');
	localStorage.removeItem('remove-me');
	t.equal(localStorage.getItem('remove-me'), null, 'getItem returns null after removeItem');
	localStorage.clear();
});

test('localStorage length', (t) => {
	t.equal(localStorage.length, 0, 'length is 0 when empty');
	localStorage.setItem('a', '1');
	localStorage.setItem('b', '2');
	t.equal(localStorage.length, 2, 'length reflects number of items');
	localStorage.clear();
});

test('localStorage key()', (t) => {
	localStorage.setItem('only-key', 'val');
	const k = localStorage.key(0);
	t.equal(k, 'only-key', 'key(0) returns the key name');
	t.equal(localStorage.key(999), null, 'key() out of range returns null');
	localStorage.clear();
});

test('localStorage clear', (t) => {
	localStorage.setItem('x', '1');
	localStorage.setItem('y', '2');
	localStorage.clear();
	t.equal(localStorage.length, 0, 'length is 0 after clear');
	t.equal(localStorage.getItem('x'), null, 'items gone after clear');
});

test('in operator returns true after setItem (proxy has)', (t) => {
	localStorage.setItem('exists', 'yes');
	t.equal('exists' in localStorage, true, '"exists" in localStorage is true');
	localStorage.clear();
});

test('in operator returns false after removeItem (proxy has)', (t) => {
	localStorage.setItem('temp', 'val');
	localStorage.removeItem('temp');
	t.equal('temp' in localStorage, false, '"temp" in localStorage is false after remove');
	localStorage.clear();
});

test('localStorage proxy get returns undefined for nonexistent', (t) => {
	localStorage.clear();
	t.equal((localStorage as any).nonexistent, undefined, 'nonexistent property is undefined');
	localStorage.clear();
});

test('localStorage proxy set via bracket notation', (t) => {
	(localStorage as any)['proxy-key'] = 'proxy-value';
	t.equal(localStorage.getItem('proxy-key'), 'proxy-value', 'bracket set works');
	localStorage.clear();
});

test('localStorage proxy delete via bracket notation', (t) => {
	localStorage.setItem('del-key', 'val');
	delete (localStorage as any)['del-key'];
	t.equal(localStorage.getItem('del-key'), null, 'delete removes the item');
	localStorage.clear();
});
