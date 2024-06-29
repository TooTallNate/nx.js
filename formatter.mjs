// @ts-check
import { fileURLToPath } from 'node:url';
import { dirname, relative } from 'node:path';
import { MarkdownPageEvent } from 'typedoc-plugin-markdown';

/**
 * @param {import('typedoc-plugin-markdown').MarkdownApplication} app
 */
export function load(app) {
	const root = fileURLToPath(new URL('./', import.meta.url));

	// Set "title" frontmatter for each page
	app.renderer.on(
		MarkdownPageEvent.BEGIN,
		/** @param {import('typedoc-plugin-markdown').MarkdownPageEvent} page */
		(page) => {
			page.frontmatter = {
				title: page.model.name,
			};
		},
	);

	// Resolve all Markdown links to root relative
	app.renderer.on(
		MarkdownPageEvent.END,
		/** @param {import('typedoc-plugin-markdown').MarkdownPageEvent} page */
		(page) => {
			if (!page.contents) return;
			const rel = relative(root, dirname(page.filename)).split('/').slice(1).join('/');
			page.contents = page.contents.replace(
				/\[(.*?)\]\((.*?)\)/g
				, (_, text, link) => {
				if (!link.includes('://')) {
					link = new URL(link, `http://e.com/docs/api/${rel}`).pathname;
				}
				return `[${text}](${link})`;
			});
		},
	);

	// Remove ".mdx" from the file extension for each link on every page
	app.renderer.on(
		MarkdownPageEvent.END,
		/** @param {import('typedoc-plugin-markdown').MarkdownPageEvent} page */
		(page) => {
			if (!page.contents) return;
			page.contents = page.contents.replace(/\.mdx/g, '');
		},
	);
}
