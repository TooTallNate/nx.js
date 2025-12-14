import { defineDocs, defineConfig } from 'fumadocs-mdx/config';

export const clkrstDocs = defineDocs({
	dir: 'content/clkrst',
});

export const constantsDocs = defineDocs({
	dir: 'content/constants',
});

export const httpDocs = defineDocs({
	dir: 'content/http',
});

export const inspectDocs = defineDocs({
	dir: 'content/inspect',
});

export const ncmDocs = defineDocs({
	dir: 'content/ncm',
});

export const replDocs = defineDocs({
	dir: 'content/repl',
});

export const runtimeDocs = defineDocs({
	dir: 'content/runtime',
});

export const utilDocs = defineDocs({
	dir: 'content/util',
});

export default defineConfig({
	mdxOptions: {
		// MDX options can be added here
	},
});
