import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('URL');

/**
 * url-constructor.any.js
 */
interface TestData {
	input: string;
	base: string;
	href: string;
	origin: string;
	protocol: string;
	username: string;
	password: string;
	host: string;
	hostname: string;
	port: string;
	pathname: string;
	search: string;
	hash: string;
	failure?: boolean;
	skip?: string;
}
const decoder = new TextDecoder();
const testData: TestData[] = JSON.parse(
	decoder.decode(Switch.readFileSync('romfs:/url/urltestdata.json')!),
).filter((t: any) => typeof t !== 'string');

for (const expected of testData) {
	const base = expected.base !== null ? expected.base : undefined;

	(expected.skip ? test.skip : test)(
		`Parsing: <${expected.input}> ${
			base ? 'against <' + base + '>' : 'without base'
		}`,
		() => {
			if (expected.failure) {
				assert.throws(() => {
					new URL(expected.input, base);
				}, TypeError);
				return;
			}

			const url = new URL(expected.input, base);
			assert.equal(url.href, expected.href, 'href');
			assert.equal(url.protocol, expected.protocol, 'protocol');
			assert.equal(url.username, expected.username, 'username');
			assert.equal(url.password, expected.password, 'password');
			assert.equal(url.host, expected.host, 'host');
			assert.equal(url.hostname, expected.hostname, 'hostname');
			assert.equal(url.port, expected.port, 'port');
			assert.equal(url.pathname, expected.pathname, 'pathname');
			assert.equal(url.search, expected.search, 'search');
			if ('searchParams' in expected) {
				assert.ok('searchParams' in url);
				assert.equal(
					url.searchParams.toString(),
					expected.searchParams,
					'searchParams',
				);
			}
			assert.equal(url.hash, expected.hash, 'hash');
		},
	);
}

/**
 * urlsearchparams-sort.any.js
 */
[
	{
		input: 'z=b&a=b&z=a&a=a',
		output: [
			['a', 'b'],
			['a', 'a'],
			['z', 'b'],
			['z', 'a'],
		],
	},
	{
		input: '\uFFFD=x&\uFFFC&\uFFFD=a',
		output: [
			['\uFFFC', ''],
			['\uFFFD', 'x'],
			['\uFFFD', 'a'],
		],
	},
	// XXX: This one might be a bug in "ada"
	//{
	//	input: 'ﬃ&🌈', // 🌈 > code point, but < code unit because two code units
	//	output: [
	//		['🌈', ''],
	//		['ﬃ', ''],
	//	],
	//},
	{
		input: 'é&e\uFFFD&e\u0301',
		output: [
			['e\u0301', ''],
			['e\uFFFD', ''],
			['é', ''],
		],
	},
	{
		input: 'z=z&a=a&z=y&a=b&z=x&a=c&z=w&a=d&z=v&a=e&z=u&a=f&z=t&a=g',
		output: [
			['a', 'a'],
			['a', 'b'],
			['a', 'c'],
			['a', 'd'],
			['a', 'e'],
			['a', 'f'],
			['a', 'g'],
			['z', 'z'],
			['z', 'y'],
			['z', 'x'],
			['z', 'w'],
			['z', 'v'],
			['z', 'u'],
			['z', 't'],
		],
	},
	{
		input: 'bbb&bb&aaa&aa=x&aa=y',
		output: [
			['aa', 'x'],
			['aa', 'y'],
			['aaa', ''],
			['bb', ''],
			['bbb', ''],
		],
	},
	{
		input: 'z=z&=f&=t&=x',
		output: [
			['', 'f'],
			['', 't'],
			['', 'x'],
			['z', 'z'],
		],
	},
	{
		input: 'a🌈&a💩',
		output: [
			['a🌈', ''],
			['a💩', ''],
		],
	},
].forEach((val) => {
	test('Parse and sort: ' + val.input, () => {
		let params = new URLSearchParams(val.input),
			i = 0;
		params.sort();
		for (let param of params) {
			assert.equal(param, val.output[i]);
			i++;
		}
	});

	test('URL parse and sort: ' + val.input, () => {
		let url = new URL('?' + val.input, 'https://example/');
		url.searchParams.sort();
		let params = new URLSearchParams(url.search),
			i = 0;
		for (let param of params) {
			assert.equal(param, val.output[i]);
			i++;
		}
	});
});

test('Sorting non-existent params removes ? from URL', function () {
	const url = new URL('http://example.com/?');
	url.searchParams.sort();
	assert.equal(url.href, 'http://example.com/');
	assert.equal(url.search, '');
});

/**
 * url-searchparams.any.js
 */
test('URL.searchParams getter', () => {
	var url = new URL('http://example.org/?a=b');
	assert.ok('searchParams' in url);
	var searchParams = url.searchParams;
	assert.equal(url.searchParams, searchParams, 'Object identity should hold.');
});

test('URL.searchParams updating, clearing', () => {
	var url = new URL('http://example.org/?a=b');
	assert.ok('searchParams' in url);
	var searchParams = url.searchParams;
	assert.equal(searchParams.toString(), 'a=b');

	searchParams.set('a', 'b');
	assert.equal(url.searchParams.toString(), 'a=b');
	assert.equal(url.search, '?a=b');
	url.search = '';
	assert.equal(url.searchParams.toString(), '');
	assert.equal(url.search, '');
	assert.equal(searchParams.toString(), '');
});

test('URL.searchParams setter, invalid values', () => {
	'use strict';
	var urlString = 'http://example.org';
	var url = new URL(urlString);
	assert.throws(() => {
		// @ts-expect-error
		url.searchParams = new URLSearchParams(urlString);
	}, TypeError);
});

test('URL.searchParams and URL.search setters, update propagation', () => {
	var url = new URL('http://example.org/file?a=b&c=d');

	assert.ok('searchParams' in url);
	var searchParams = url.searchParams;
	assert.equal(url.search, '?a=b&c=d');
	assert.equal(searchParams.toString(), 'a=b&c=d');

	// Test that setting 'search' propagates to the URL object's query object.
	url.search = 'e=f&g=h';
	assert.equal(url.search, '?e=f&g=h');
	assert.equal(searchParams.toString(), 'e=f&g=h');

	// ..and same but with a leading '?'.
	url.search = '?e=f&g=h';
	assert.equal(url.search, '?e=f&g=h');
	assert.equal(searchParams.toString(), 'e=f&g=h');

	// And in the other direction, altering searchParams propagates
	// back to 'search'.
	searchParams.append('i', ' j ');
	assert.equal(url.search, '?e=f&g=h&i=+j+');
	assert.equal(url.searchParams.toString(), 'e=f&g=h&i=+j+');
	assert.equal(searchParams.get('i'), ' j ');

	searchParams.set('e', 'updated');
	assert.equal(url.search, '?e=updated&g=h&i=+j+');
	assert.equal(searchParams.get('e'), 'updated');

	var url2 = new URL('http://example.org/file??a=b&c=d');
	assert.equal(url2.search, '??a=b&c=d');
	assert.equal(url2.searchParams.toString(), '%3Fa=b&c=d');

	url2.href = 'http://example.org/file??a=b';
	assert.equal(url2.search, '??a=b');
	assert.equal(url2.searchParams.toString(), '%3Fa=b');
});

/**
 * urlsearchparams-append.any.js
 */
test('Append same name', () => {
	var params = new URLSearchParams();
	params.append('a', 'b');
	assert.is(params + '', 'a=b');
	params.append('a', 'b');
	assert.is(params + '', 'a=b&a=b');
	params.append('a', 'c');
	assert.is(params + '', 'a=b&a=b&a=c');
});

test('Append empty strings', () => {
	var params = new URLSearchParams();
	params.append('', '');
	assert.is(params + '', '=');
	params.append('', '');
	assert.is(params + '', '=&=');
});

test('Append null', () => {
	var params = new URLSearchParams();
	// @ts-expect-error
	params.append(null, null);
	assert.is(params + '', 'null=null');
	// @ts-expect-error
	params.append(null, null);
	assert.is(params + '', 'null=null&null=null');
});

test('Append multiple', () => {
	var params = new URLSearchParams();
	// @ts-expect-error
	params.append('first', 1);
	// @ts-expect-error
	params.append('second', 2);
	params.append('third', '');
	// @ts-expect-error
	params.append('first', 10);
	assert.ok(params.has('first'), 'Search params object has name "first"');
	assert.is(
		params.get('first'),
		'1',
		'Search params object has name "first" with value "1"',
	);
	assert.is(
		params.get('second'),
		'2',
		'Search params object has name "second" with value "2"',
	);
	assert.is(
		params.get('third'),
		'',
		'Search params object has name "third" with value ""',
	);
	// @ts-expect-error
	params.append('first', 10);
	assert.is(
		params.get('first'),
		'1',
		'Search params object has name "first" with value "1"',
	);
});

/**
 * urlsearchparams-delete.any.js
 */
test('Delete basics', function () {
	var params = new URLSearchParams('a=b&c=d');
	params.delete('a');
	assert.is(params + '', 'c=d');
	params = new URLSearchParams('a=a&b=b&a=a&c=c');
	params.delete('a');
	assert.is(params + '', 'b=b&c=c');
	params = new URLSearchParams('a=a&=&b=b&c=c');
	params.delete('');
	assert.is(params + '', 'a=a&b=b&c=c');
	params = new URLSearchParams('a=a&null=null&b=b');
	// @ts-expect-error
	params.delete(null);
	assert.is(params + '', 'a=a&b=b');
	params = new URLSearchParams('a=a&undefined=undefined&b=b');
	// @ts-expect-error
	params.delete(undefined);
	assert.is(params + '', 'a=a&b=b');
});

test('Deleting appended multiple', function () {
	var params = new URLSearchParams();
	// @ts-expect-error
	params.append('first', 1);
	assert.ok(params.has('first'), 'Search params object has name "first"');
	assert.is(
		params.get('first'),
		'1',
		'Search params object has name "first" with value "1"',
	);
	params.delete('first');
	assert.not(params.has('first'), 'Search params object has no "first" name');
	// @ts-expect-error
	params.append('first', 1);
	// @ts-expect-error
	params.append('first', 10);
	params.delete('first');
	assert.not(params.has('first'), 'Search params object has no "first" name');
});

test('Deleting all params removes ? from URL', function () {
	var url = new URL('http://example.com/?param1&param2');
	url.searchParams.delete('param1');
	url.searchParams.delete('param2');
	assert.is(url.href, 'http://example.com/', 'url.href does not have ?');
	assert.is(url.search, '', 'url.search does not have ?');
});

test('Removing non-existent param removes ? from URL', function () {
	var url = new URL('http://example.com/?');
	url.searchParams.delete('param1');
	assert.is(url.href, 'http://example.com/', 'url.href does not have ?');
	assert.is(url.search, '', 'url.search does not have ?');
});

test.skip('Changing the query of a URL with an opaque path can impact the path', () => {
	const url = new URL('data:space    ?test');
	assert.ok(url.searchParams.has('test'));
	url.searchParams.delete('test');
	assert.not(url.searchParams.has('test'));
	assert.is(url.search, '');
	assert.is(url.pathname, 'space');
	assert.is(url.href, 'data:space');
});

test('Changing the query of a URL with an opaque path can impact the path if the URL has no fragment', () => {
	const url = new URL('data:space    ?test#test');
	url.searchParams.delete('test');
	assert.is(url.search, '');
	assert.is(url.pathname, 'space    ');
	assert.is(url.href, 'data:space    #test');
});

test('Two-argument delete()', () => {
	const params = new URLSearchParams();
	params.append('a', 'b');
	params.append('a', 'c');
	params.append('a', 'd');
	params.delete('a', 'c');
	assert.is(params.toString(), 'a=b&a=d');
});

test('Two-argument delete() respects undefined as second arg', () => {
	const params = new URLSearchParams();
	params.append('a', 'b');
	params.append('a', 'c');
	params.append('b', 'c');
	params.append('b', 'd');
	params.delete('b', 'c');
	params.delete('a', undefined);
	assert.is(params.toString(), 'b=d');
});

test.run();
