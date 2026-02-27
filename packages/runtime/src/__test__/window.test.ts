import { describe, it, expect } from 'vitest';

describe('window', () => {
	it('atob', () => {
		expect(globalThis.hasOwnProperty('atob')).toBe(true);
		expect(atob.length).toBe(1);
		const result = atob('SGVsbG8sIHdvcmxk');
		expect(result).toBe('Hello, world');
	});

	it('btoa', () => {
		expect(globalThis.hasOwnProperty('btoa')).toBe(true);
		expect(btoa.length).toBe(1);
		const result = btoa('Hello, world');
		expect(result).toBe('SGVsbG8sIHdvcmxk');
	});
});
