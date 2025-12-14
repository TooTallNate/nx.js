import { createMDX } from 'fumadocs-mdx/next';

const withMDX = createMDX();

/** @type {import('next').NextConfig} */
const config = {
	reactStrictMode: true,
	redirects() {
		return [
			{
				statusCode: 301,
				source: '/tests/redirect/301',
				destination: 'https://dump.n8.io',
			},
			{
				statusCode: 302,
				source: '/tests/redirect/302',
				destination: 'https://dump.n8.io',
			},
			{
				statusCode: 307,
				source: '/tests/redirect/307',
				destination: 'https://dump.n8.io',
			},
			{
				statusCode: 308,
				source: '/tests/redirect/308',
				destination: 'https://dump.n8.io',
			},
		];
	},
};

export default withMDX(config);
