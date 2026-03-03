import { test } from '../src/tap';

test('set() replaces first and removes all subsequent duplicates', (t) => {
	const fd = new FormData();
	fd.append('key', 'a');
	fd.append('key', 'b');
	fd.append('key', 'c');
	fd.set('key', 'replaced');
	const all = fd.getAll('key');
	t.equal(all.length, 1, 'should have exactly one entry');
	t.equal(all[0], 'replaced', 'value should be replaced');
});

test('set() with new name appends', (t) => {
	const fd = new FormData();
	fd.set('a', '1');
	fd.set('b', '2');
	t.equal(fd.get('a'), '1');
	t.equal(fd.get('b'), '2');
});

test('set() preserves order of first entry', (t) => {
	const fd = new FormData();
	fd.append('a', '1');
	fd.append('b', '2');
	fd.append('a', '3');
	fd.set('a', 'replaced');
	const keys = [...fd.keys()];
	t.equal(keys.length, 2);
	t.equal(keys[0], 'a');
	t.equal(keys[1], 'b');
});

test('append() does not remove duplicates', (t) => {
	const fd = new FormData();
	fd.append('key', 'a');
	fd.append('key', 'b');
	const all = fd.getAll('key');
	t.equal(all.length, 2);
	t.equal(all[0], 'a');
	t.equal(all[1], 'b');
});

test('Blob default filename is "blob"', (t) => {
	const fd = new FormData();
	const blob = new Blob(['hello']);
	fd.set('file', blob);
	const f = fd.get('file') as File;
	t.equal(f.name, 'blob', 'default filename should be "blob"');
});

test('Blob default filename is "blob" with append', (t) => {
	const fd = new FormData();
	const blob = new Blob(['hello']);
	fd.append('file', blob);
	const f = fd.get('file') as File;
	t.equal(f.name, 'blob', 'default filename should be "blob"');
});

test('Blob with explicit filename', (t) => {
	const fd = new FormData();
	const blob = new Blob(['hello']);
	fd.set('file', blob, 'custom.txt');
	const f = fd.get('file') as File;
	t.equal(f.name, 'custom.txt');
});

test('File preserves original filename', (t) => {
	const fd = new FormData();
	const file = new File(['data'], 'original.txt');
	fd.set('file', file);
	const f = fd.get('file') as File;
	t.equal(f.name, 'original.txt');
});

test('get/getAll/has/delete behavior', (t) => {
	const fd = new FormData();
	t.equal(fd.get('x'), null, 'get missing returns null');
	t.equal(fd.has('x'), false, 'has missing returns false');
	fd.append('x', 'a');
	fd.append('x', 'b');
	t.equal(fd.get('x'), 'a', 'get returns first');
	t.equal(fd.has('x'), true);
	t.equal(fd.getAll('x').length, 2);
	fd.delete('x');
	t.equal(fd.has('x'), false, 'deleted');
	t.equal(fd.getAll('x').length, 0);
});

test('entries/keys/values iteration', (t) => {
	const fd = new FormData();
	fd.append('a', '1');
	fd.append('b', '2');
	const entries = [...fd.entries()];
	t.equal(entries.length, 2);
	t.equal(entries[0][0], 'a');
	t.equal(entries[1][0], 'b');
	const keys = [...fd.keys()];
	t.equal(keys[0], 'a');
	t.equal(keys[1], 'b');
	const vals = [...fd.values()];
	t.equal(vals[0], '1');
	t.equal(vals[1], '2');
});

test('forEach iteration order', (t) => {
	const fd = new FormData();
	fd.append('a', '1');
	fd.append('b', '2');
	fd.append('c', '3');
	const collected: string[] = [];
	fd.forEach((value, key) => {
		collected.push(`${key}=${value}`);
	});
	t.equal(collected[0], 'a=1');
	t.equal(collected[1], 'b=2');
	t.equal(collected[2], 'c=3');
});
