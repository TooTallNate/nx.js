import { pageTree } from '../source';
import { DocsLayout } from 'fumadocs-ui/layout';
import Logo from '@/app/logo';
import type { ReactNode } from 'react';

export default function RootDocsLayout({ children }: { children: ReactNode }) {
	return (
		<DocsLayout
			tree={pageTree}
			nav={{
				transparentMode: 'top',
				title: (
					<>
						<Logo className='w-5 md:w-6' /> nx.js
					</>
				),
				githubUrl: 'https://github.com/TooTallNate/nx.js',
				//children: ['a'],
			}}
		>
			{children}
		</DocsLayout>
	);
}
