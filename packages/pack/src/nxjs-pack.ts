#!/usr/bin/env node
import bytes from 'bytes';
import chalk from 'chalk';
import {
	writeFileSync,
	readdirSync,
	readFileSync,
	statSync,
	existsSync,
} from 'node:fs';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { relative } from 'node:path';
import parseAuthor from 'parse-author';
import { NACP } from '@tootallnate/nacp';
import * as NRO from '@tootallnate/nro';
import * as RomFS from '@tootallnate/romfs';
import type { PackageJson } from 'types-package-json';

const cwd = process.cwd();
const packageRoot = new URL(`${pathToFileURL(cwd)}/`);

// Read the `package.json` file to get the information that will
// be embedded in the NACP section of the output `.nro` file
const packageJsonUrl = new URL('package.json', packageRoot);
const packageJsonStr = readFileSync(packageJsonUrl, 'utf8');
const packageJson: PackageJson & { titleId?: string } =
	JSON.parse(packageJsonStr);
const { name, version, author: rawAuthor } = packageJson;
const author =
	typeof rawAuthor === 'string' ? parseAuthor(rawAuthor) : rawAuthor;

const isSrcMode = existsSync(new URL('../tsconfig.json', import.meta.url));
const nxjsNroUrl = new URL(
	isSrcMode ? '../../../nxjs.nro' : '../dist/nxjs.nro',
	import.meta.url
);
const nxjsNroBuffer = readFileSync(nxjsNroUrl);
const nxjsNroBlob = new Blob([nxjsNroBuffer]);
const nxjsNro = await NRO.decode(nxjsNroBlob);

// Icon
let icon = nxjsNro.icon;
console.log(chalk.bold('Loading Icon:'));
const iconName = 'icon.jpg';
try {
	const iconUrl = new URL('icon.jpg', packageRoot);
	const iconData = readFileSync(iconUrl);
	icon = new Blob([iconData]);
	console.log(
		`  Using ${chalk.bold(`"${iconName}"`)} file (${bytes(
			icon.size
		).toLowerCase()})`
	);
} catch (err: any) {
	if (err.code !== 'ENOENT') {
		throw err;
	}
	console.log(
		`  ⚠️ No ${chalk.bold(
			`"${iconName}"`
		)} file found. Default nx.js icon will be used.`
	);
}

// NACP
const nacp = new NACP(await nxjsNro.nacp!.arrayBuffer());
console.log();
console.log(chalk.bold('Setting metadata:'));
if (packageJson.titleId) {
	nacp.id = packageJson.titleId;
	console.log(`  ID: ${chalk.green(nacp.id.toString(16))}`);
}
console.log(
	`  Title: ${chalk.strikethrough.red(nacp.title)} → ${chalk.green(name)}`
);
nacp.title = name;
if (version) {
	console.log(
		`  Version: ${chalk.strikethrough.red(nacp.version)} → ${chalk.green(
			version
		)}`
	);
	nacp.version = version;
} else {
	console.log(`  Version: ${chalk.green(nacp.version)}`);
}
if (author?.name) {
	console.log(
		`  Author: ${chalk.strikethrough.red(nacp.author)} → ${chalk.green(
			author
		)}`
	);
	nacp.author = author.name;
} else {
	console.log(`  Author: ${chalk.green(nacp.author)}`);
}

// RomFS
const romfsDir = new URL('romfs/', packageRoot);
const romfsDirPath = fileURLToPath(romfsDir);
const romfs = await RomFS.decode(nxjsNro.romfs!);
console.log();
console.log(chalk.bold('Adding RomFS files:'));

function walk(dir: URL, dirEntry: RomFS.RomFsEntry) {
	for (const name of readdirSync(dir)) {
		const fileUrl = new URL(name, dir);
		const stat = statSync(fileUrl);
		if (stat.isDirectory()) {
			let entry = dirEntry[name] ?? Object.create(null);
			if (entry instanceof Blob) {
				throw new Error(`Expected directory: ${fileUrl}`);
			}
			dirEntry[name] = entry;
			walk(new URL(`${fileUrl}/`), entry);
		} else if (stat.isFile()) {
			const blob = new Blob([readFileSync(fileUrl)]);
			console.log(
				`  ${chalk.cyan(
					relative(romfsDirPath, fileURLToPath(fileUrl))
				)} (${bytes(blob.size).toLowerCase()})`
			);
			dirEntry[name] = blob;
		} else {
			throw new Error(`Unsupported file type: ${fileUrl}`);
		}
	}
}

try {
	walk(romfsDir, romfs);
} catch (err: any) {
	// Don't crash if there is no `romfs` directory
	if (err.code !== 'ENOENT') throw err;
}

const outputNroName = `${name}.nro`;

if (!(romfs['main.js'] instanceof Blob)) {
	console.log();
	console.log(
		chalk.yellow(
			`${chalk.bold(
				'Warning!'
			)} No "main.js" file found in \`romfs\` directory.`
		)
	);
	console.log(
		chalk.yellow(
			`The entrypoint file ${chalk.bold(`"${name}.js"`)} will need to`
		)
	);
	console.log(
		chalk.yellow(
			`be placed alongside ${chalk.bold(
				`"${outputNroName}"`
			)} on the SD card.`
		)
	);
}

// Generate final NRO file
const outputNro = await NRO.encode({
	...nxjsNro,
	icon,
	nacp: new Blob([new Uint8Array(nacp.buffer)]),
	romfs: await RomFS.encode(romfs),
});
const outputNroUrl = new URL(outputNroName, packageRoot);
writeFileSync(outputNroUrl, Buffer.from(await outputNro.arrayBuffer()));
console.log(
	chalk.green(
		`\n🎉 Success! Generated NRO file ${chalk.bold(`"${outputNroName}"`)}`
	) + ` (${bytes(outputNro.size).toLowerCase()})`
);
