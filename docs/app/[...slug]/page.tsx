import { DocsLayout } from 'fumadocs-ui/layouts/docs';
import { DocsBody, DocsPage } from 'fumadocs-ui/layouts/docs/page';
import type { Metadata } from 'next';
import { notFound } from 'next/navigation';
import Logo from '@/app/logo';
import { loaders } from '@/app/source';
import { useMDXComponents } from '@/mdx-components';
import Discord from '../discord';
import GbaTemp from '../gbatemp';

const sidebarTabs = [
	{
		title: 'Runtime',
		description: 'Global APIs',
		url: '/runtime',
	},
	{
		title: '@nx.js/clkrst',
		description: 'Overclocking API',
		url: '/clkrst',
	},
	{
		title: '@nx.js/constants',
		description: 'Constants and enums',
		url: '/constants',
	},
	{
		title: '@nx.js/http',
		description: 'HTTP server for nx.js',
		url: '/http',
	},
	{
		title: '@nx.js/inspect',
		description: 'Inspection utility for nx.js',
		url: '/inspect',
	},
	{
		title: '@nx.js/ncm',
		description: 'Content manager API',
		url: '/ncm',
	},
	{
		title: '@nx.js/repl',
		description: 'Read-Eval-Print Loop for nx.js',
		url: '/repl',
	},
	{
		title: '@nx.js/ws',
		description: 'WebSocket server for nx.js',
		url: '/ws',
	},
];

export default async function Page(props: {
	params: Promise<{ slug?: string[] }>;
}) {
	const params = await props.params;
	if (!params.slug) {
		notFound();
	}

	const root = params.slug[0];
	const loader = loaders.get(root);
	if (!loader) {
		notFound();
	}

	const page = loader.getPage(params.slug.slice(1));

	if (page == null) {
		notFound();
	}

	const MDX = page.data.body;
	const components = useMDXComponents({});

	return (
		<DocsLayout
			tree={loader.pageTree}
			githubUrl='https://github.com/TooTallNate/nx.js'
			nav={{
				transparentMode: 'top',
				title: (
					<>
						<Logo className='w-5 md:w-6 drop-shadow' /> nx.js
					</>
				),
			}}
			links={[
				{
					icon: <Discord className='fill-current' />,
					text: 'Join the Discord server',
					url: 'https://discord.gg/MMmn73nsGz',
				},
				{
					icon: <GbaTemp className='fill-current' />,
					text: 'GBATemp thread',
					url: 'https://gbatemp.net/threads/nx-js-javascript-runtime-for-nintendo-switch-homebrew-applications.639171/',
				},
			]}
			sidebar={{
				defaultOpenLevel: 0,
				tabs: sidebarTabs,
			}}
		>
			<DocsPage toc={page.data.toc.filter((t) => t.depth <= 3)}>
				<DocsBody>
					<h1>{page.data.title}</h1>
					<p className='mb-8 text-lg text-fd-muted-foreground'>
						{page.data.description}
					</p>
					<MDX components={components} />
				</DocsBody>
			</DocsPage>
		</DocsLayout>
	);
}

export async function generateStaticParams() {
	return Array.from(loaders.entries()).flatMap(([root, loader]) =>
		loader.getPages().map((page) => ({
			slug: [root].concat(page.slugs),
		})),
	);
}

export async function generateMetadata(props: {
	params: Promise<{ slug?: string[] }>;
}) {
	const params = await props.params;
	if (!params.slug) {
		notFound();
	}

	const root = params.slug[0];
	const loader = loaders.get(root);
	if (!loader) {
		notFound();
	}

	const page = loader.getPage(params.slug.slice(1));

	if (page == null) notFound();

	return {
		title: page.data.title,
		description: page.data.description,
	} satisfies Metadata;
}
