#!/usr/bin/env node
import chalk from 'chalk';
import degit from 'degit';
import which from 'which';
import { once } from 'events';
import { promises as fs } from 'fs';
import { spawn } from 'child_process';
import terminalLink from 'terminal-link';
import { fileURLToPath, pathToFileURL } from 'url';
import { input, select, Separator } from '@inquirer/prompts';
import type { PackageJson } from 'types-package-json';

async function hasPackageManager(name: string) {
	const p = await which(name, { nothrow: true });
	return p ? name : null;
}

const packageManagersPromise = Promise.all([
	hasPackageManager('pnpm'),
	hasPackageManager('yarn'),
	hasPackageManager('npm'),
]);

// Ask which example app to clone
const distDir = new URL('../dist/', import.meta.url);
const { version } = JSON.parse(
	await fs.readFile(new URL('../package.json', import.meta.url), 'utf8')
);
const choices: { name: string; value: string; description: string }[] =
	JSON.parse(await fs.readFile(new URL('choices.json', distDir), 'utf8'));
for (const c of choices) {
	c.value = c.name;
	c.name = terminalLink(
		c.name,
		`https://github.com/TooTallNate/nx.js/tree/v${version}/apps/${c.name}`,
		{ fallback: false }
	);
}
const template = await select({
	message: 'Select a nx.js template:',
	choices,
});

// Ask name of user's application
const appName = await input({
	message: 'Enter the name of your application:',
	default: template,
});

// Begin cloning example to project directory
const appDir = new URL(`${appName}/`, pathToFileURL(process.cwd() + '/'));
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
	(p) => p !== null
) as string[];
const packageManager = await select({
	message: 'Install dependencies using which package manager?',
	choices: [
		...packageManagers.map((name) => {
			return { name, value: name };
		}),
		new Separator(),
		{
			name: 'skip',
			value: 'skip',
			description: 'Dependencies will not be installed',
		},
	],
});

// Wait for cloning to complete
const cloneError = await clonePromise;
if (cloneError) {
	console.error(`There was an error during the cloning process:`);
	console.error(cloneError);
	process.exit(1);
}

function removeWorkspace(deps: Record<string, string> = {}) {
	for (const [name, value] of Object.entries(deps)) {
		const noWorkspace = value.replace(/^workspace:\*?/, '');
		if (noWorkspace !== value) {
			deps[name] = `${noWorkspace}${version}`;
		}
	}
}

// Patch the `package.json` file with the app name, and update any
// dependencies that use "workspace:" in the version to the actual version
const packageJsonUrl = new URL('package.json', appDir);
const packageJson: PackageJson = JSON.parse(await fs.readFile(packageJsonUrl, 'utf8'));
packageJson.name = appName;
packageJson.description = '';
removeWorkspace(packageJson.dependencies);
removeWorkspace(packageJson.devDependencies);
await fs.writeFile(packageJsonUrl, `${JSON.stringify(packageJson, null, 2)}\n`);

// Install dependencies
if (packageManager !== 'skip') {
	const cp = spawn(packageManager, ['install'], {
		stdio: 'inherit',
		cwd: appDir,
	});
	const [exitCode] = await once(cp, 'exit');
	if (exitCode !== 0) {
		process.exit(exitCode);
	}
}

// Ok, we're done
console.log(
	chalk.green(
		`\n🎉 nx.js app ${chalk.bold(`"${appName}"`)} initialized successfully!`
	)
);
console.log();
console.log(chalk.bold('Next steps'));
console.log();
const cmd = chalk.yellow;
if (packageManager === 'skip') {
	console.log(` • Run the following command: ${cmd(`cd ${appName}`)}`);
	console.log(` • Install dependencies using your preferred package manager`);
	console.log(` • Invoke the ${cmd('build')} script to bundle your app`);
	console.log(` • Invoke the ${cmd('nro')} script to generate a NRO file`);
} else {
	console.log(` • Run the following commands:`);
	console.log();
	console.log(`     ${cmd(`cd ${appName}`)}`);
	console.log(`     ${cmd(`${packageManager} run build`)}`);
	console.log(`     ${cmd(`${packageManager} run nro`)}`);
}
