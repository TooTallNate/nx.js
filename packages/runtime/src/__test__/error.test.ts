import { describe, it, expect } from 'vitest';

describe('Error', () => {
	it('Catch immediately thrown errors in async function', async () => {
		async function immediatelyThrow() {
			throw new Error('should be caught');
		}
		let err: Error | undefined;
		try {
			await immediatelyThrow();
		} catch (_err: any) {
			err = _err;
		}
		expect(err).toBeDefined();
		expect(err!.message).toBe('should be caught');
	});

	it('Catch asynchronously thrown errors in async function', async () => {
		async function throwAfterTick() {
			await Promise.resolve();
			throw new Error('should be caught');
		}
		let err: Error | undefined;
		try {
			await throwAfterTick();
		} catch (_err: any) {
			err = _err;
		}
		expect(err).toBeDefined();
		expect(err!.message).toBe('should be caught');
	});
});
