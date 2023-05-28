import { readFileSync } from 'node:fs';

export function getGitTag() {
	const packageJsonUrl = new URL(
		'../../packages/runtime/package.json',
		import.meta.url
	);
	const { version } = JSON.parse(readFileSync(packageJsonUrl, 'utf8'));
	return `v${version}`;
}

export async function createRelease({ github, context }) {
	const {
		repo: { owner, repo },
	} = context;

	const nxjsNroUrl = new URL('../../nxjs.nro', import.meta.url);
	const nxjsNroBuffer = readFileSync(nxjsNroUrl)

	const tag = getGitTag();
	const releaseName = `nx.js ${tag}`;

	// TODO: fill this out
	const releaseBody = undefined;

	// Create the release
	const releaseResponse = await github.rest.repos.createRelease({
		owner,
		repo,
		tag_name: tag,
		name: releaseName,
		body: releaseBody,
	});
	console.log(
		`Created Release "${
			releaseResponse.data.name || releaseResponse.data.tag_name
		}"`
	);

	// Upload the file to the release
	const uploadResponse = await github.rest.repos.uploadReleaseAsset({
		owner,
		repo,
		release_id: releaseResponse.data.id,
		name: 'nxjs.nro',
		data: nxjsNroBuffer,
	});
	console.log(
		`Uploaded "nxjs.nro" Release Asset: ${uploadResponse.data.browser_download_url}`
	);
}
