import parseAuthor from 'parse-author';
import { readFileSync } from 'node:fs';
import { NACP } from '@tootallnate/nacp';
import { titleCase } from 'title-case';
import type { PackageJson as BasePackageJson } from 'types-package-json';

export type PackageJsonNacp = Omit<NACP, 'buffer'>;

export interface PackageJson extends BasePackageJson {
	titleId?: string;
	productName?: string;
	nacp?: PackageJsonNacp;
}

const VALID_NACP_PROPERTIES = Object.getOwnPropertyNames(NACP.prototype);

export function patchNACP(nacp: NACP, packageJsonUrl: URL) {
	const warnings: string[] = [];
	const updated = new Map<string, string>();
	const packageJsonStr = readFileSync(packageJsonUrl, 'utf8');
	const packageJson: PackageJson = JSON.parse(packageJsonStr);
	const {
		titleId,
		name,
		productName,
		version,
		author: rawAuthor,
		nacp: pkgNacp = {},
	} = packageJson;

	if (titleId) {
		warnings.push(
			'The "titleId" property is deprecated. Use "nacp.id" instead.',
		);
		nacp.id = titleId;
		updated.set('ID', titleId);
	}

	const title = productName || name;
	if (title) {
		if (productName) {
			warnings.push(
				'The "productName" property is deprecated. Use "nacp.title" instead.',
			);
		}
		updated.set('Title', title);
		nacp.title = title;
	}

	if (version) {
		nacp.version = version;
		updated.set('Version', version);
	}

	const author =
		typeof rawAuthor === 'string' ? parseAuthor(rawAuthor) : rawAuthor;
	if (author?.name) {
		nacp.author = author.name;
		updated.set('Author', author.name);
	}

	for (const [k, v] of Object.entries(pkgNacp)) {
		if (!VALID_NACP_PROPERTIES.includes(k)) {
			warnings.push(`Ignoring invalid NACP property: ${JSON.stringify(k)}`);
			continue;
		}

		// @ts-expect-error
		const oldValue = nacp[k];
		// @ts-expect-error
		nacp[k] = v;
		// @ts-expect-error
		const newValue = nacp[k];

		if (newValue !== oldValue) {
			const titleCased =
				k === 'id' ? 'ID' : titleCase(k.replace(/([A-Z])/g, ' $1'));
			let label = newValue;
			if (k === 'userAccountSaveDataSize' && newValue === 0n) {
				label = `${newValue} (\`localStorage\` disabled)`;
			}
			updated.set(titleCased, label);
		}
	}

	return { packageJson, updated, warnings };
}
