import parseAuthor from 'parse-author';
import { readFileSync } from 'node:fs';
import { NACP, VideoCapture } from '@tootallnate/nacp';
import { titleCase } from 'title-case';
import type { PackageJson as BasePackageJson } from 'types-package-json';

export type PackageJsonNacp = Omit<NACP, 'buffer'>;

// TODO: move this to `@tootallnate/nacp`
enum Screenshot {
	Enabled,
	Disabled,
}

export interface PackageJson extends BasePackageJson {
	/**
	 * @deprecated Use `nacp.id` instead.
	 */
	titleId?: string;
	/**
	 * @deprecated Use `nacp.title` instead.
	 */
	productName?: string;
	/**
	 * Additional NACP properties to set.
	 */
	nacp?: PackageJsonNacp;
}

export type NacpProperty = Exclude<keyof NACP, 'buffer'>;

const VALID_NACP_PROPERTIES = Object.getOwnPropertyNames(NACP.prototype);

const parseBigInt = (k: string, v: unknown): bigint => {
	if (typeof v === 'number') return BigInt(v);
	if (typeof v === 'string') return BigInt(v);
	throw new Error(`Invalid BigInt value for "${k}": ${v}`);
};

const parseTitleId = (k: string, v: unknown): bigint => {
	if (typeof v === 'string') {
		if (v.length !== 16) {
			throw new Error(`"${k}" must be 16 hexadecimal digits`);
		}
		return BigInt(`0x${v}`);
	}
	throw new Error(`Invalid Title ID value for "${k}": ${v}`);
};

const parseEnum =
	<
		T extends {
			[K in keyof T]: K extends string ? number : string;
		},
	>(
		Enum: T,
		trueValue?: keyof T,
		falseValue?: keyof T,
	) =>
	(k: string, v: unknown): number => {
		let val: number | undefined;
		if (typeof v === 'string') {
			val = Enum[v as keyof T] as number;
		} else if (typeof v === 'boolean') {
			val = Enum[v ? trueValue : falseValue] as number;
		} else if (typeof v === 'number') {
			val = v;
		}
		if (
			typeof val === 'undefined' ||
			typeof Enum[val as keyof T] !== 'string'
		) {
			throw new Error(`Invalid "${k}" value: ${v}`);
		}
		return val;
	};

const parsers: {
	[T in NacpProperty]?: (k: T, v: unknown) => NACP[T];
} = {
	id: parseTitleId,
	saveDataOwnerId: parseTitleId,
	userAccountSaveDataSize: parseBigInt,
	videoCapture: parseEnum(VideoCapture, 'Enabled', 'Disabled'),
	screenshot: parseEnum(Screenshot, 'Enabled', 'Disabled'),
};

const formatTitleId = (v: bigint) => v.toString(16).padStart(16, '0');

const formatters: {
	[T in NacpProperty]?: (v: NACP[T]) => string;
} = {
	id: formatTitleId,
	saveDataOwnerId: formatTitleId,
	userAccountSaveDataSize: (v) =>
		v === 0n ? `${v} (\`localStorage\` disabled)` : String(v),
	videoCapture: (v) => VideoCapture[v],
	screenshot: (v) => Screenshot[v],
};

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
		updated.set('ID', getFormatter('id')(nacp.id));
	}

	const title = productName || name;
	if (title) {
		if (productName) {
			warnings.push(
				'The "productName" property is deprecated. Use "nacp.title" instead.',
			);
		}
		nacp.title = title;
		updated.set('Title', getFormatter('title')(nacp.title));
	}

	if (version) {
		nacp.version = version;
		updated.set('Version', getFormatter('version')(nacp.version));
	}

	const author =
		typeof rawAuthor === 'string' ? parseAuthor(rawAuthor) : rawAuthor;
	if (author?.name) {
		nacp.author = author.name;
		updated.set('Author', getFormatter('author')(nacp.author));
	}

	for (const [k, v] of Object.entries(pkgNacp)) {
		if (!VALID_NACP_PROPERTIES.includes(k)) {
			warnings.push(`Ignoring invalid NACP property: ${JSON.stringify(k)}`);
			continue;
		}

		const newValue = updateValue(nacp, k as NacpProperty, v);

		const titleCased =
			k === 'id' ? 'ID' : titleCase(k.replace(/([A-Z])/g, ' $1'));
		const formatter = getFormatter(k as NacpProperty);
		updated.set(titleCased, formatter(newValue));
	}

	return { packageJson, updated, warnings };
}

function getFormatter<T extends NacpProperty>(key: T): (v: NACP[T]) => string {
	return formatters[key] ?? String;
}

function updateValue<T extends NacpProperty>(
	nacp: NACP,
	key: T,
	value: unknown,
): NACP[T] {
	const parser = parsers[key] ?? ((_, v): NACP[T] => v as NACP[T]);
	nacp[key] = parser(key, value);
	return nacp[key];
}
