import { execSync } from 'child_process';
import { getGitTag } from './create-release.mjs';

const tag = getGitTag();
try {
	execSync(`git tag ${tag}`, { stdio: 'pipe' });
	// This log line is important. `changesets/action` checks for this
	// specific format in order to set the "published" output, which we
	// rely on in order to trigger the job to create the GitHub Release.
	// https://github.com/changesets/action/blob/595655c3eae7136ff5ba18200406898904362926/src/run.ts#LL96C1-L96C1
	console.log(`New tag: nxjs-runtime@${tag}`);
} catch (err) {
	if (err.message.includes('already exists')) {
		console.log(`Skipping since git tag "${tag}" already exists`);
	} else {
		throw err;
	}
}
