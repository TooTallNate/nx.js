import { describe, it, expect } from 'vitest';

describe('URL', () => {
	it('URLSearchParams iterator functions', async () => {
		const p = new URLSearchParams('a=a&a=b&b=c&c=d');
		expect(Array.from(p.keys())).toEqual(['a', 'a', 'b', 'c']);
		expect(Array.from(p.values())).toEqual(['a', 'b', 'c', 'd']);
		expect(Array.from(p.entries())).toEqual([
			['a', 'a'],
			['a', 'b'],
			['b', 'c'],
			['c', 'd'],
		]);
		expect(Array.from(p)).toEqual([
			['a', 'a'],
			['a', 'b'],
			['b', 'c'],
			['c', 'd'],
		]);

		const it2 = p.keys();
		await new Promise((r) => setTimeout(r, 100));
		expect(Array.from(it2)).toEqual(['a', 'a', 'b', 'c']);
	});

	describe('urlsearchparams-sort', () => {
		const sortTests = [
			{
				input: 'z=b&a=b&z=a&a=a',
				output: [['a', 'b'], ['a', 'a'], ['z', 'b'], ['z', 'a']],
			},
			{
				input: '\uFFFD=x&\uFFFC&\uFFFD=a',
				output: [['\uFFFC', ''], ['\uFFFD', 'x'], ['\uFFFD', 'a']],
			},
			{
				input: 'é&e\uFFFD&e\u0301',
				output: [['e\u0301', ''], ['e\uFFFD', ''], ['é', '']],
			},
			{
				input: 'z=z&a=a&z=y&a=b&z=x&a=c&z=w&a=d&z=v&a=e&z=u&a=f&z=t&a=g',
				output: [
					['a', 'a'], ['a', 'b'], ['a', 'c'], ['a', 'd'],
					['a', 'e'], ['a', 'f'], ['a', 'g'],
					['z', 'z'], ['z', 'y'], ['z', 'x'], ['z', 'w'],
					['z', 'v'], ['z', 'u'], ['z', 't'],
				],
			},
			{
				input: 'bbb&bb&aaa&aa=x&aa=y',
				output: [['aa', 'x'], ['aa', 'y'], ['aaa', ''], ['bb', ''], ['bbb', '']],
			},
			{
				input: 'z=z&=f&=t&=x',
				output: [['', 'f'], ['', 't'], ['', 'x'], ['z', 'z']],
			},
			{
				input: 'a🌈&a💩',
				output: [['a🌈', ''], ['a💩', '']],
			},
		];

		for (const val of sortTests) {
			it(`Parse and sort: ${val.input}`, () => {
				const params = new URLSearchParams(val.input);
				let i = 0;
				params.sort();
				for (const param of params) {
					expect(param).toEqual(val.output[i]);
					i++;
				}
			});

			it(`URL parse and sort: ${val.input}`, () => {
				const url = new URL('?' + val.input, 'https://example/');
				url.searchParams.sort();
				const params = new URLSearchParams(url.search);
				let i = 0;
				for (const param of params) {
					expect(param).toEqual(val.output[i]);
					i++;
				}
			});
		}

		it('Sorting non-existent params removes ? from URL', () => {
			const url = new URL('http://example.com/?');
			url.searchParams.sort();
			expect(url.href).toBe('http://example.com/');
			expect(url.search).toBe('');
		});
	});

	describe('URL.searchParams', () => {
		it('getter', () => {
			const url = new URL('http://example.org/?a=b');
			expect('searchParams' in url).toBe(true);
			const searchParams = url.searchParams;
			expect(url.searchParams).toBe(searchParams);
		});

		it('updating, clearing', () => {
			const url = new URL('http://example.org/?a=b');
			const searchParams = url.searchParams;
			expect(searchParams.toString()).toBe('a=b');

			searchParams.set('a', 'b');
			expect(url.searchParams.toString()).toBe('a=b');
			expect(url.search).toBe('?a=b');
			url.search = '';
			expect(url.searchParams.toString()).toBe('');
			expect(url.search).toBe('');
			expect(searchParams.toString()).toBe('');
		});

		it('setter, invalid values', () => {
			'use strict';
			const url = new URL('http://example.org');
			expect(() => {
				// @ts-expect-error
				url.searchParams = new URLSearchParams('http://example.org');
			}).toThrow(TypeError);
		});

		it('and URL.search setters, update propagation', () => {
			const url = new URL('http://example.org/file?a=b&c=d');
			const searchParams = url.searchParams;
			expect(url.search).toBe('?a=b&c=d');
			expect(searchParams.toString()).toBe('a=b&c=d');

			url.search = 'e=f&g=h';
			expect(url.search).toBe('?e=f&g=h');
			expect(searchParams.toString()).toBe('e=f&g=h');

			url.search = '?e=f&g=h';
			expect(url.search).toBe('?e=f&g=h');
			expect(searchParams.toString()).toBe('e=f&g=h');

			searchParams.append('i', ' j ');
			expect(url.search).toBe('?e=f&g=h&i=+j+');
			expect(url.searchParams.toString()).toBe('e=f&g=h&i=+j+');
			expect(searchParams.get('i')).toBe(' j ');

			searchParams.set('e', 'updated');
			expect(url.search).toBe('?e=updated&g=h&i=+j+');
			expect(searchParams.get('e')).toBe('updated');

			const url2 = new URL('http://example.org/file??a=b&c=d');
			expect(url2.search).toBe('??a=b&c=d');
			expect(url2.searchParams.toString()).toBe('%3Fa=b&c=d');

			url2.href = 'http://example.org/file??a=b';
			expect(url2.search).toBe('??a=b');
			expect(url2.searchParams.toString()).toBe('%3Fa=b');
		});
	});

	describe('urlsearchparams-append', () => {
		it('Append same name', () => {
			const params = new URLSearchParams();
			params.append('a', 'b');
			expect(params + '').toBe('a=b');
			params.append('a', 'b');
			expect(params + '').toBe('a=b&a=b');
			params.append('a', 'c');
			expect(params + '').toBe('a=b&a=b&a=c');
		});

		it('Append empty strings', () => {
			const params = new URLSearchParams();
			params.append('', '');
			expect(params + '').toBe('=');
			params.append('', '');
			expect(params + '').toBe('=&=');
		});

		it('Append null', () => {
			const params = new URLSearchParams();
			// @ts-expect-error
			params.append(null, null);
			expect(params + '').toBe('null=null');
			// @ts-expect-error
			params.append(null, null);
			expect(params + '').toBe('null=null&null=null');
		});

		it('Append multiple', () => {
			const params = new URLSearchParams();
			// @ts-expect-error
			params.append('first', 1);
			// @ts-expect-error
			params.append('second', 2);
			params.append('third', '');
			// @ts-expect-error
			params.append('first', 10);
			expect(params.has('first')).toBe(true);
			expect(params.get('first')).toBe('1');
			expect(params.get('second')).toBe('2');
			expect(params.get('third')).toBe('');
			// @ts-expect-error
			params.append('first', 10);
			expect(params.get('first')).toBe('1');
		});
	});

	describe('urlsearchparams-delete', () => {
		it('Delete basics', () => {
			let params = new URLSearchParams('a=b&c=d');
			params.delete('a');
			expect(params + '').toBe('c=d');
			params = new URLSearchParams('a=a&b=b&a=a&c=c');
			params.delete('a');
			expect(params + '').toBe('b=b&c=c');
			params = new URLSearchParams('a=a&=&b=b&c=c');
			params.delete('');
			expect(params + '').toBe('a=a&b=b&c=c');
			params = new URLSearchParams('a=a&null=null&b=b');
			// @ts-expect-error
			params.delete(null);
			expect(params + '').toBe('a=a&b=b');
			params = new URLSearchParams('a=a&undefined=undefined&b=b');
			// @ts-expect-error
			params.delete(undefined);
			expect(params + '').toBe('a=a&b=b');
		});

		it('Deleting appended multiple', () => {
			const params = new URLSearchParams();
			// @ts-expect-error
			params.append('first', 1);
			expect(params.has('first')).toBe(true);
			expect(params.get('first')).toBe('1');
			params.delete('first');
			expect(params.has('first')).toBe(false);
			// @ts-expect-error
			params.append('first', 1);
			// @ts-expect-error
			params.append('first', 10);
			params.delete('first');
			expect(params.has('first')).toBe(false);
		});

		it('Deleting all params removes ? from URL', () => {
			const url = new URL('http://example.com/?param1&param2');
			url.searchParams.delete('param1');
			url.searchParams.delete('param2');
			expect(url.href).toBe('http://example.com/');
			expect(url.search).toBe('');
		});

		it('Removing non-existent param removes ? from URL', () => {
			const url = new URL('http://example.com/?');
			url.searchParams.delete('param1');
			expect(url.href).toBe('http://example.com/');
			expect(url.search).toBe('');
		});

		it('Changing the query of a URL with an opaque path can impact the path if the URL has no fragment', () => {
			const url = new URL('data:space    ?test#test');
			url.searchParams.delete('test');
			expect(url.search).toBe('');
			expect(url.pathname).toBe('space    ');
			expect(url.href).toBe('data:space    #test');
		});

		it('Two-argument delete()', () => {
			const params = new URLSearchParams();
			params.append('a', 'b');
			params.append('a', 'c');
			params.append('a', 'd');
			params.delete('a', 'c');
			expect(params.toString()).toBe('a=b&a=d');
		});

		it('Two-argument delete() respects undefined as second arg', () => {
			const params = new URLSearchParams();
			params.append('a', 'b');
			params.append('a', 'c');
			params.append('b', 'c');
			params.append('b', 'd');
			params.delete('b', 'c');
			params.delete('a', undefined);
			expect(params.toString()).toBe('b=d');
		});
	});
});
