import './global.css';
import { RootProvider } from 'fumadocs-ui/provider/next';
import localFont from 'next/font/local';
import { GeistSans } from 'geist/font/sans';
import { GeistMono } from 'geist/font/mono';
import { Analytics } from '@vercel/analytics/react';

const iconsFont = localFont({
	src: './icons.woff2',
	display: 'swap',
	variable: '--font-icons',
});

export default async function Layout({
	children,
}: Readonly<{
	children: React.ReactNode;
}>) {
	return (
		<html
			lang="en"
			className={`${GeistSans.variable} ${GeistMono.variable} ${iconsFont.variable}`}
			suppressHydrationWarning
		>
			<body>
				<RootProvider>{children}</RootProvider>
				<Analytics />
			</body>
		</html>
	);
}
