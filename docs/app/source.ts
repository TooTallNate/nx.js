import { join } from 'node:path';
import { readdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { map } from '@/.map';
import { createMDXSource } from 'fumadocs-mdx';
import { loader } from 'fumadocs-core/source';

const contentDir = join(fileURLToPath(import.meta.url), '../../content');

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
