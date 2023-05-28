/**
 * `changeset version` updates the version and adds a changelog file in
 * the example apps, but we don't want to do that. So this script reverts
 * any "version" field changes and deletes the `CHANGELOG.md` file.
 */

import { readFileSync, writeFileSync, unlinkSync, readdirSync } from 'node:fs';

const appsUrl = new URL('../../apps/', import.meta.url);

for (const app of readdirSync(appsUrl)) {
	const packageJsonUrl = new URL(`${app}/package.json`, appsUrl);
	const packageJson = JSON.parse(readFileSync(packageJsonUrl, 'utf8'));
	packageJson.version = '0.0.0';
	writeFileSync(packageJsonUrl, JSON.stringify(packageJson, null, 2) + '\n');

	try {
		const changelogUrl = new URL(`${app}/CHANGELOG.md`, appsUrl);
		unlinkSync(changelogUrl);
	} catch (err) {
		if (err.code !== 'ENOENT') throw err;
	}
}
