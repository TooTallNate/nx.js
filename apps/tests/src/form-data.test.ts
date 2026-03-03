import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('FormData');

test('FormData url encoded', async () => {
	const r = new Response('a=a&a=b&b=c&c=d', {
		headers: {
			'content-type': 'application/x-www-form-urlencoded',
		},
	});
	const form = await r.formData();
	assert.instance(form, FormData);
	assert.equal(form.get('a'), 'a');
	assert.equal(form.get('b'), 'c');
	assert.equal(form.get('c'), 'd');
	assert.equal(form.getAll('a'), ['a', 'b']);
	assert.equal(form.getAll('b'), ['c']);
	assert.equal(form.getAll('c'), ['d']);
	assert.equal(form.get('missing'), null);
});

test('FormData multipart', async () => {
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
	assert.instance(form2, FormData);
	assert.equal(form2.get('string'), 'string-value');
	assert.equal(form2.get('missing'), null);

	const file2 = form2.get('file');
	assert.ok(file2 && typeof file2 !== 'string');
	assert.instance(file2, File);
	assert.equal(file2.name, 'file.txt');
	assert.equal(file2.type, 'text/plain');
	assert.equal(await file2.text(), 'contents');
});

test('FormData append Blob gets default filename "blob"', () => {
	const form = new FormData();
	const blob = new Blob(['hello'], { type: 'text/plain' });

	form.append('file', blob);
	const entry = form.get('file');
	assert.ok(entry && typeof entry !== 'string');
	assert.instance(entry, File);
	assert.equal(entry.name, 'blob');
	assert.equal(entry.type, 'text/plain');
});

test('FormData append Blob with custom filename', () => {
	const form = new FormData();
	const blob = new Blob(['hello'], { type: 'text/plain' });

	form.append('file', blob, 'custom.txt');
	const entry = form.get('file');
	assert.ok(entry && typeof entry !== 'string');
	assert.instance(entry, File);
	assert.equal(entry.name, 'custom.txt');
});

test('FormData set Blob gets default filename "blob"', () => {
	const form = new FormData();
	const blob = new Blob(['hello'], { type: 'application/octet-stream' });

	form.set('file', blob);
	const entry = form.get('file');
	assert.ok(entry && typeof entry !== 'string');
	assert.instance(entry, File);
	assert.equal(entry.name, 'blob');
});

test('FormData set() removes duplicate entries', () => {
	const form = new FormData();
	form.append('key', 'first');
	form.append('key', 'second');
	form.append('key', 'third');
	form.append('other', 'keep');

	assert.equal(form.getAll('key').length, 3);

	form.set('key', 'replaced');

	assert.equal(form.getAll('key'), ['replaced']);
	assert.equal(form.get('key'), 'replaced');
	assert.equal(form.get('other'), 'keep');
});

test('FormData set() replaces at first occurrence position', () => {
	const form = new FormData();
	form.append('a', '1');
	form.append('key', 'first');
	form.append('b', '2');
	form.append('key', 'second');
	form.append('c', '3');

	form.set('key', 'replaced');

	// Verify order: a, key(replaced), b, c — 'second' entry removed
	const entries = [...form.entries()];
	assert.equal(entries.length, 4);
	assert.equal(entries[0], ['a', '1']);
	assert.equal(entries[1], ['key', 'replaced']);
	assert.equal(entries[2], ['b', '2']);
	assert.equal(entries[3], ['c', '3']);
});

test('FormData set() adds entry if name does not exist', () => {
	const form = new FormData();
	form.append('existing', 'value');

	form.set('newkey', 'newvalue');

	assert.equal(form.get('newkey'), 'newvalue');
	assert.equal(form.getAll('newkey'), ['newvalue']);
});

test.run();
