import { pageTree } from '../source';
import { DocsLayout } from 'fumadocs-ui/layout';
import Logo from '@/app/logo';
import type { ReactNode } from 'react';

export default function RootDocsLayout({ children }: { children: ReactNode }) {
	return (
		<DocsLayout
			tree={pageTree}
			githubUrl={'https://github.com/TooTallNate/nx.js'}
			nav={{
				transparentMode: 'top',
				title: (
					<>
						<Logo className='w-5 md:w-6 drop-shadow' /> nx.js
					</>
				),
				//children: ['a'],
			}}
		>
			{children}
		</DocsLayout>
	);
}
