import { describe, it, expect } from 'vitest';

describe('navigator', () => {
	it('instanceof', () => {
		expect(navigator instanceof Navigator).toBe(true);
	});

	it('throws illegal constructor', () => {
		let err: Error | undefined;
		try {
			new Navigator();
		} catch (e: any) {
			err = e;
		}
		expect(err).toBeDefined();
		expect(err!.message).toBe('Illegal constructor');
	});
});
