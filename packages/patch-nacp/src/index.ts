import parseAuthor from 'parse-author';
import { readFileSync } from 'node:fs';
import type { NACP } from '@tootallnate/nacp';
import type { PackageJson as BasePackageJson } from 'types-package-json';

export interface PackageJson extends BasePackageJson {
	titleId?: string;
	productName?: string;
}

export function patchNACP(nacp: NACP, packageJsonUrl: URL) {
	const packageJsonStr = readFileSync(packageJsonUrl, 'utf8');
	const packageJson: PackageJson = JSON.parse(packageJsonStr);
	const {
		titleId,
		name,
		productName,
		version,
		author: rawAuthor,
	} = packageJson;
	const author =
		typeof rawAuthor === 'string' ? parseAuthor(rawAuthor) : rawAuthor;
	if (titleId) nacp.id = titleId;
	const title = productName || name;
	if (title) nacp.title = title;
	if (version) nacp.version = version;
	if (author?.name) nacp.author = author.name;
}
