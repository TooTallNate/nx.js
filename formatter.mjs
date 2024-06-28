// @ts-check
import { relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
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

	// Resolve all links to root relative
	app.renderer.on(
		MarkdownPageEvent.END,
		/** @param {import('typedoc-plugin-markdown').MarkdownPageEvent} page */
		(page) => {
			if (!page.contents) return;
			const rel = relative(root, page.filename);
			const pkg = rel.split('/')[1];
			page.contents = page.contents.replace(
				/\[(.*?)\]\((.*?)\)/g
				, (_, text, link) => {
				if (!link.includes('://')) {
					link = resolve(`/docs/api/${pkg}`, link);
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
