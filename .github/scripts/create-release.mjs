import yauzl from 'yauzl';
import { readFileSync } from 'node:fs';

async function findWorkflowForSha({ octokit, context, name }) {
	const {
		sha,
		repo: { owner, repo },
	} = context;

	// Get the workflow runs for the repository
	const response = await octokit.actions.listWorkflowRunsForRepo({
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

async function findArtifactForWorkflow({ octokit, context, workflow, name }) {
	const {
		repo: { owner, repo },
	} = context;

	// Get the artifact details
	const response = await octokit.actions.listWorkflowRunArtifacts({
		owner,
		repo,
		run_id: workflow.id,
	});

	// Find the artifact by name
	const artifact = response.data.artifacts.find(
		(artifact) => artifact.name === name
	);

	if (!artifact) {
		throw new Error(
			`Could not find artifact "${name}" for workflow run ${workflow.id}`
		);
	}

	return artifact;
}

async function extractArtifact({ octokit, context, artifact }) {
	const {
		repo: { owner, repo },
	} = context;

	// Download the artifact file
	const artifactResponse = await octokit.actions.downloadArtifact({
		owner,
		repo,
		artifact_id: artifact.id,
		archive_format: 'zip',
	});

	const stream = await new Promise((resolve, reject) =>
		yauzl.fromBuffer(
			Buffer.from(artifactResponse.data),
			{ lazyEntries: true },
			(err, zipFile) => {
				if (err) return reject(err);
				zipFile.readEntry();
				zipFile.on('entry', (entry) => {
					if (entry.fileName === artifact.name) {
						zipFile.openReadStream(entry, (err, s) => {
							if (err) return reject(err);
							resolve(s);
						});
					} else {
						zipFile.readEntry();
					}
				});
			}
		)
	);

	const chunks = [];
	for await (const chunk of stream) {
		chunks.push(chunk);
	}

	return Buffer.concat(chunks);
}

export function getGitTag() {
	const packageJsonUrl = new URL(
		'../../packages/runtime/package.json',
		import.meta.url
	);
	const { version } = JSON.parse(readFileSync(packageJsonUrl, 'utf8'));
	return `v${version}`;
}

export async function createRelease({ octokit, context }) {
	const workflow = await findWorkflowForSha({
		octokit,
		context,
		name: 'CI',
	});
	const artifact = await findArtifactForWorkflow({
		octokit,
		context,
		workflow,
		name: 'nxjs.nro',
	});
	const nxjsNroBuffer = await extractArtifact({ octokit, context, artifact });

	const tag = getGitTag();
	const releaseName = `nx.js ${tag}`;

	// TODO: fill this out
	const releaseBody = undefined;

	// Create the release
	const releaseResponse = await octokit.repos.createRelease({
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
	const uploadResponse = await octokit.repos.uploadReleaseAsset({
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
