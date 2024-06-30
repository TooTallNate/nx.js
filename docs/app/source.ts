import { join } from 'node:path';
import { readdirSync } from 'node:fs';
import { map } from '@/.map';
import { createMDXSource } from 'fumadocs-mdx';
import { loader } from 'fumadocs-core/source';

const dirnameParts = __dirname.split('/');
const docsDir = dirnameParts.slice(0, dirnameParts.indexOf('docs') + 1).join('/');
const contentDir = join(docsDir, 'content');

export const loaders: Map<
	string,
	ReturnType<typeof loader<{ source: ReturnType<typeof createMDXSource> }>>
> = new Map(
	readdirSync(contentDir).map((name) => [
		name,
		loader({
			baseUrl: `/${name}`,
			rootDir: name,
			source: createMDXSource(map),
		}),
	]),
);
