import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('FormData');

test('FormData', async () => {
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
