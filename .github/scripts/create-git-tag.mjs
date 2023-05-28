import { execSync } from 'child_process';
import { getGitTag } from './create-release.mjs';

const tag = getGitTag();
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
