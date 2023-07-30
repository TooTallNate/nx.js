import degit from 'degit';
import { fileURLToPath, pathToFileURL } from 'url';
import { promises as fs } from 'fs';
import { input, select } from '@inquirer/prompts';

const distDir = new URL('../dist/', import.meta.url);
const { version } = JSON.parse(
	await fs.readFile(new URL('../package.json', import.meta.url), 'utf8')
);
const choices: { name: string; value: string; description: string }[] =
	JSON.parse(await fs.readFile(new URL('choices.json', distDir), 'utf8'));
for (const c of choices) { c.value = c.name }

const template = await select({
	message: 'Select a nx.js template:',
	choices,
});

const appName = await input({
	message: 'Enter the name of your application:',
	default: template,
});

const dir = new URL(appName, pathToFileURL(process.cwd() + '/'));
await fs.mkdir(dir);

const emitter = degit(`TooTallNate/nx.js/apps/${template}#v${version}`);

emitter.on('info', info => {
	console.log(info);
});

await emitter.clone(fileURLToPath(dir));
