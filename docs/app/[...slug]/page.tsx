import Logo from '@/app/logo';
import { loaders } from '@/app/source';
import { Toggle } from '@/app/toggle';
import { DocsPage, DocsBody } from 'fumadocs-ui/page';
import { notFound } from 'next/navigation';
import { DocsLayout } from 'fumadocs-ui/layout';
import Discord from '../discord';
import GbaTemp from '../gbatemp';
import type { Metadata } from 'next';

export default async function Page({
	params,
}: {
	params: { slug?: string[] };
}) {
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

	const MDX = page.data.exports.default;

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
				banner: <Toggle />,
			}}
		>
			<DocsPage toc={page.data.exports.toc.filter((t) => t.depth <= 3)}>
				<DocsBody>
					<h1>{page.data.title}</h1>
					<p className='mb-8 text-lg text-muted-foreground'>
						{page.data.description}
					</p>
					<MDX />
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

export function generateMetadata({ params }: { params: { slug?: string[] } }) {
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
