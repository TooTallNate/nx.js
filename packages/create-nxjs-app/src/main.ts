#!/usr/bin/env node
import chalk from 'chalk';
import degit from 'degit';
import which from 'which';
import { once } from 'events';
import { promises as fs } from 'fs';
import { fileURLToPath, pathToFileURL } from 'url';
import { SpawnOptions, spawn } from 'child_process';
import slugify from '@sindresorhus/slugify';
import terminalLink from 'terminal-link';
import * as clack from '@clack/prompts';
import type { PackageJson } from 'types-package-json';

interface Data {
	apps: { name: string; value: string; description: string }[];
	packages: { [name: string]: { version: string } };
}

function generateRandomID() {
	let titleId = '01';
	for (let i = 0; i < 10; i++) {
		titleId += Math.floor(Math.random() * 16).toString(16);
	}
	titleId += '0000';
	return titleId;
}

async function hasPackageManager(name: string) {
	const p = await which(name, { nothrow: true });
	return p ? name : null;
}

function removeWorkspace(
	deps: Record<string, string> = {},
	packages: Data['packages'],
) {
	for (const [name, value] of Object.entries(deps)) {
		const noWorkspace = value.replace(/^workspace:\*?/, '');
		if (noWorkspace !== value) {
			const { version } = packages[name];
			deps[name] = `${noWorkspace}${version}`;
		}
	}
}

async function exec(cmd: string, args: string[], opts: SpawnOptions = {}) {
	const c = spawn(cmd, args, opts);
	let stdout = '';
	if (c.stdout) {
		c.stdout.setEncoding('utf8');
		c.stdout.on('data', (d) => {
			stdout += d;
		});
	}
	const [exitCode] = await once(c, 'exit');
	if (exitCode !== 0) {
		throw new Error(`Command "${cmd} ${args.join(' ')}" exit code ${exitCode}`);
	}
	return stdout;
}

const packageManagersPromise = Promise.all([
	hasPackageManager('pnpm'),
	hasPackageManager('yarn'),
	hasPackageManager('npm'),
	hasPackageManager('bun'),
]);

clack.intro(chalk.bold('Create nx.js app'));

try {
	// Ask which example app to clone
	const distDir = new URL('../dist/', import.meta.url);
	const { version } = JSON.parse(
		await fs.readFile(new URL('../package.json', import.meta.url), 'utf8'),
	);
	const data: Data = JSON.parse(
		await fs.readFile(new URL('data.json', distDir), 'utf8'),
	);
	const template = await clack.select({
		message: 'Select a nx.js template:',
		options: data.apps.map(({ name, description }) => {
			return {
				label: terminalLink(
					name,
					`https://github.com/TooTallNate/nx.js/tree/v${version}/apps/${name}`,
					{ fallback: false },
				),
				value: name,
				hint: description,
			};
		}),
	});
	if (clack.isCancel(template)) {
		throw template;
	}

	// Ask name of user's application
	const appName = await clack.text({
		message: 'Enter the name of your application:',
		defaultValue: template,
		placeholder: template,
	});
	if (clack.isCancel(appName)) {
		throw appName;
	}

	const slugifiedName = slugify(appName);

	// Begin cloning example to project directory
	const appDir = new URL(
		`${slugifiedName}/`,
		pathToFileURL(process.cwd() + '/'),
	);
	await fs.mkdir(appDir, { recursive: true });
	const emitter = degit(`TooTallNate/nx.js/apps/${template}#v${version}`);
	emitter.on('info', (info) => {
		if (info.code !== 'SUCCESS') {
			console.log(`${info.code}: ${info.message}`);
		}
	});
	const clonePromise = emitter.clone(fileURLToPath(appDir)).catch((err) => err);

	// Ask which package manager to use. This happens
	// in parallel of cloning the example application.
	const packageManagers = (await packageManagersPromise).filter(
		(p) => p !== null,
	) as string[];
	const packageManager = await clack.select({
		message: 'Install dependencies using which package manager?',
		options: [
			...packageManagers.map((label) => {
				return { label, value: label };
			}),
			{
				label: 'skip',
				value: 'skip',
				hint: 'dependencies will not be installed',
			},
		],
	});
	if (clack.isCancel(packageManager)) {
		throw packageManager;
	}

	// Wait for cloning to complete
	const spinner = clack.spinner();
	spinner.start(
		`Cloning \`${template}\` template into "${slugifiedName}" directory`,
	);
	const cloneError = await clonePromise;
	if (cloneError) {
		console.error(`There was an error during the cloning process:`);
		console.error(cloneError);
		process.exit(1);
	}
	spinner.stop(
		`Cloned \`${template}\` template into "${slugifiedName}" directory`,
	);

	// Patch the `package.json` file with the app name, and update any
	// dependencies that use "workspace:" in the version to the actual version
	const packageJsonUrl = new URL('package.json', appDir);
	const [packageJson, gitName, gitEmail] = await Promise.all([
		fs
			.readFile(packageJsonUrl, 'utf8')
			.then((p) => JSON.parse(p) as PackageJson),
		exec('git', ['config', '--global', 'user.name'], {
			stdio: 'pipe',
		})
			.then((v) => v.trim())
			.catch(() => undefined),
		exec('git', ['config', '--global', 'user.email'], {
			stdio: 'pipe',
		})
			.then((v) => v.trim())
			.catch(() => undefined),
	]);
	packageJson.name = slugifiedName;
	packageJson.description = '';

	// Add "author" field based on git config data
	if (gitName) {
		packageJson.author = {
			name: gitName,
			email: gitEmail,
		};
	}

	removeWorkspace(packageJson.dependencies, data.packages);
	removeWorkspace(packageJson.devDependencies, data.packages);
	await fs.writeFile(
		packageJsonUrl,
		`${JSON.stringify(
			{
				...packageJson,
				nacp: {
					// Generate a random Title ID, which is used for
					// save data purposes (i.e. `localStorage`)
					id: generateRandomID(),

					// Set the `nacp.title` to the app name if it's different
					// from the slugified name, for usage in NRO/NSP app metadata
					title: appName !== slugifiedName ? appName : undefined,
				},
			},
			null,
			2,
		)}\n`,
	);

	// Install dependencies
	if (packageManager !== 'skip') {
		const spinner = clack.spinner();
		spinner.start(`Installing dependencies via ${packageManager}`);
		// TODO: figure out why `packageManager` is not inferred as "string"
		const cp = spawn(packageManager as string, ['install'], {
			cwd: appDir,
			shell: true,
		});
		const [exitCode] = await once(cp, 'exit');
		if (exitCode !== 0) {
			process.exit(exitCode);
		}
		spinner.stop(`Installed dependencies via ${packageManager}`);
	}

	// Ok, we're done
	clack.outro(
		chalk.green(
			`🎉 nx.js app ${chalk.bold(`"${appName}"`)} initialized successfully!`,
		),
	);
	console.log(chalk.bold('Next steps'));
	console.log();
	const cmd = chalk.yellow;
	if (packageManager === 'skip') {
		console.log(` • Run the following command: ${cmd(`cd ${slugifiedName}`)}`);
		console.log(` • Install dependencies using your preferred package manager`);
		console.log(` • Invoke the ${cmd('build')} script to bundle your app`);
		console.log(` • Invoke the ${cmd('nro')} script to generate a NRO file`);
	} else {
		console.log(` • Run the following commands:`);
		console.log();
		console.log(`     ${cmd(`cd ${slugifiedName}`)}`);
		console.log(`     ${cmd(`${packageManager} run build`)}`);
		console.log(`     ${cmd(`${packageManager} run nro`)}`);
	}
} catch (err: unknown) {
	if (!clack.isCancel(err)) throw err;
	clack.cancel('Operation cancelled.');
}
