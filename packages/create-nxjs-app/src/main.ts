#!/usr/bin/env node
import degit from 'degit';
import { promises as fs } from 'fs';
import { fileURLToPath, pathToFileURL } from 'url';
import { input, select } from '@inquirer/prompts';

const distDir = new URL('../dist/', import.meta.url);
const { version } = JSON.parse(
	await fs.readFile(new URL('../package.json', import.meta.url), 'utf8')
);
const choices: { name: string; value: string; description: string }[] =
	JSON.parse(await fs.readFile(new URL('choices.json', distDir), 'utf8'));
for (const c of choices) {
	c.value = c.name;
}

const template = await select({
	message: 'Select a nx.js template:',
	choices,
});

const appName = await input({
	message: 'Enter the name of your application:',
	default: template,
});

const appDir = new URL(`${appName}/`, pathToFileURL(process.cwd() + '/'));
await fs.mkdir(appDir, { recursive: true });

const emitter = degit(`TooTallNate/nx.js/apps/${template}#v${version}`);

emitter.on('info', (info) => {
	if (info.code !== 'SUCCESS') {
		console.log(`${info.code}: ${info.message}`);
	}
});

await emitter.clone(fileURLToPath(appDir));

const pkgJsonUrl = new URL('package.json', appDir);
const pkgJson = JSON.parse(await fs.readFile(pkgJsonUrl, 'utf8'));
pkgJson.name = appName;
pkgJson.description = '';
await fs.writeFile(pkgJsonUrl, `${JSON.stringify(pkgJson, null, 2)}\n`);

console.log(`nx.js app "${appName}" initialized successfully!`);
