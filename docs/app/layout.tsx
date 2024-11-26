import './global.css';
import { RootProvider } from 'fumadocs-ui/provider';
import localFont from 'next/font/local';
import { GeistSans } from 'geist/font/sans';
import { GeistMono } from 'geist/font/mono';
import { Analytics } from '@vercel/analytics/react';
import type { ReactNode } from 'react';

const iconsFont = localFont({
	src: './icons.woff2',
	display: 'swap',
	variable: '--font-icons',
});

export default function Layout({ children }: { children: ReactNode }) {
	return (
		<html
			lang='en'
			className={`${GeistSans.variable} ${GeistMono.variable} ${iconsFont.variable}`}
		>
			<body>
				<RootProvider>{children}</RootProvider>
				<Analytics />
			</body>
		</html>
	);
}
