import { readFileSync } from 'node:fs';

async function findWorkflowForSha({ github, context, name }) {
	const {
		sha,
		repo: { owner, repo },
	} = context;

	// Get the workflow runs for the repository
	const response = await github.rest.actions.listWorkflowRunsForRepo({
		owner,
		repo,
	});

	// Find the workflow run with the matching commit SHA
	const workflowRun = response.data.workflow_runs.find(
		(run) => run.head_sha === sha && run.name === name
	);

	if (!workflowRun) {
		throw new Error(`Could not find "${name}" workflow for SHA ${sha}`);
	}

	return workflowRun;
}

export function getGitTag() {
	const packageJsonUrl = new URL(
		'../../packages/runtime/package.json',
		import.meta.url
	);
	const { version } = JSON.parse(readFileSync(packageJsonUrl, 'utf8'));
	return `v${version}`;
}

export async function createRelease({ github, context }) {
	const workflow = await findWorkflowForSha({
		github,
		context,
		name: 'CI',
	});
  console.log('Workflow ID', workflow.id);
  await new Promise(r => setTimeout(r, 60 * 1000));

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
