import { expect, test, describe } from 'vitest';
import { resolvePath } from '../src/static';

describe('resolvePath()', () => {
	const root = 'romfs:/public/';

	test.each([
		{ input: 'http://1.1.1.1:1234/', expected: 'romfs:/public/' },
		{
			input: 'http://1.1.1.1:1234/index.html',
			expected: 'romfs:/public/index.html',
		},
		{
			input: 'http://1.1.1.1:1234/foo/index.html',
			expected: 'romfs:/public/foo/index.html',
		},
		{
			input: 'http://1.1.1.1:1234/foo//index.html',
			expected: 'romfs:/public/foo//index.html',
		},
		{
			input: 'http://1.1.1.1:1234//index.html',
			expected: 'romfs:/public/index.html',
		},
		{ input: 'http://1.1.1.1:1234/..', expected: 'romfs:/public/' },
		{
			input: 'http://1.1.1.1:1234/../index.html',
			expected: 'romfs:/public/index.html',
		},
		{
			input: 'http://1.1.1.1:1234/../../index.html',
			expected: 'romfs:/public/index.html',
		},
	])('should resolve URL $input to $expected', ({ input, expected }) => {
		expect(resolvePath(input, root).href).toEqual(expected);
	});
});
