import { loaders } from '@/app/source';
import { createSearchAPI } from 'fumadocs-core/search/server';

export const { GET } = createSearchAPI('advanced', {
	indexes: Array.from(loaders.values()).flatMap((loader) =>
		loader.getPages().map((page) => ({
			title: page.data.title,
			structuredData: page.data.structuredData,
			id: page.url,
			url: page.url,
		})),
	),
});
