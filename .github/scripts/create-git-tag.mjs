import { execSync } from 'child_process';
import { readFileSync } from 'node:fs';

const packageJsonUrl = new URL(
	'../../packages/runtime/package.json',
	import.meta.url
);
const { version } = JSON.parse(readFileSync(packageJsonUrl, 'utf8'));
const tag = `v${version}`;
try {
	execSync(`git tag ${tag}`, { stdio: 'pipe' });
	console.log(`Created git tag "${tag}"`);
} catch (err) {
	if (err.message.includes('already exists')) {
		console.log(`Skipping since git tag "${tag}" already exists`);
	} else {
		throw err;
	}
}
