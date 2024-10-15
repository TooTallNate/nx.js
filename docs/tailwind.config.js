import { createPreset } from 'fumadocs-ui/tailwind-plugin';

/** @type {import('tailwindcss').Config} */
export default {
	content: [
		'./components/**/*.{ts,tsx}',
		'./app/**/*.{ts,tsx}',
		'./content/**/*.{md,mdx}',
		'./mdx-components.{ts,tsx}',
		'./node_modules/fumadocs-ui/dist/**/*.js',
	],
	theme: {
		extend: {
			fontFamily: {
				sans: ['var(--font-geist-sans)'],
				mono: ['var(--font-geist-mono)'],
			},
		},
	},
	presets: [createPreset()],
};
