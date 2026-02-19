import { defineConfig } from 'vitest/config';

export default defineConfig({
	test: {
		setupFiles: ['./setup.ts'],
		testTimeout: 30000,
		hookTimeout: 30000,
	},
});
