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

test.run();
