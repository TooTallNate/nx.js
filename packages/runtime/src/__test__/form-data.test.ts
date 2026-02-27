import { describe, it, expect } from 'vitest';

describe('FormData', () => {
	it('FormData url encoded', async () => {
		const r = new Response('a=a&a=b&b=c&c=d', {
			headers: { 'content-type': 'application/x-www-form-urlencoded' },
		});
		const form = await r.formData();
		expect(form).toBeInstanceOf(FormData);
		expect(form.get('a')).toBe('a');
		expect(form.get('b')).toBe('c');
		expect(form.get('c')).toBe('d');
		expect(form.getAll('a')).toEqual(['a', 'b']);
		expect(form.getAll('b')).toEqual(['c']);
		expect(form.getAll('c')).toEqual(['d']);
		expect(form.get('missing')).toBeNull();
	});

	it('FormData multipart', async () => {
		const file = new File(['conte', new Blob(['nts'])], 'file.txt', {
			type: 'text/plain',
		});
		expect(file.name).toBe('file.txt');
		expect(file.type).toBe('text/plain');

		const form = new FormData();
		form.append('file', file);
		form.append('string', 'string-value');

		const r = new Response(form);
		const form2 = await r.formData();
		expect(form2).toBeInstanceOf(FormData);
		expect(form2.get('string')).toBe('string-value');
		expect(form2.get('missing')).toBeNull();

		const file2 = form2.get('file');
		expect(file2).toBeTruthy();
		expect(typeof file2).not.toBe('string');
		expect(file2).toBeInstanceOf(File);
		expect((file2 as File).name).toBe('file.txt');
		expect((file2 as File).type).toBe('text/plain');
		expect(await (file2 as File).text()).toBe('contents');
	});
});
