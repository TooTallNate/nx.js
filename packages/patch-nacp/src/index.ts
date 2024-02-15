import parseAuthor from 'parse-author';
import { readFileSync } from 'node:fs';
import type { NACP } from '@tootallnate/nacp';
import type { PackageJson } from 'types-package-json';

export function patchNACP(nacp: NACP, packageJsonUrl: URL) {
	const packageJsonStr = readFileSync(packageJsonUrl, 'utf8');
	const packageJson: PackageJson & { titleId?: string } =
		JSON.parse(packageJsonStr);
	const { titleId, name, version, author: rawAuthor } = packageJson;
	const author =
		typeof rawAuthor === 'string' ? parseAuthor(rawAuthor) : rawAuthor;
	if (titleId) nacp.id = titleId;
	if (name) nacp.title = name;
	if (version) nacp.version = version;
	if (author?.name) nacp.author = author.name;
}
