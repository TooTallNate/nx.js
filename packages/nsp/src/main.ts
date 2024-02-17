#!/usr/bin/env node
import bytes from 'bytes';
import chalk from 'chalk';
import {
	cpSync,
	existsSync,
	rmSync,
	mkdtempSync,
	readFileSync,
	writeFileSync,
	readdirSync,
	statSync,
} from 'node:fs';
import { join } from 'node:path';
import { once } from 'node:events';
import { spawn } from 'node:child_process';
import { tmpdir } from 'node:os';
import { pathToFileURL, fileURLToPath } from 'node:url';
import Jimp from 'jimp';
import { NACP } from '@tootallnate/nacp';
import { patchNACP } from '@nx.js/patch-nacp';
import terminalImage from 'terminal-image';

const iconName = 'icon.jpg';
const tmpPaths: URL[] = [];
const tmpDir = tmpdir();
const cwd = process.cwd();
const root = new URL('../', import.meta.url);
const appRoot = new URL(`${pathToFileURL(cwd)}/`);
const isSrcMode = existsSync(new URL('tsconfig.json', root));

const createTmpDir = (type: string) => {
	const dir = new URL(
		`${pathToFileURL(mkdtempSync(join(tmpDir, `nxjs-nsp_${type}.`)))}/`
	);
	tmpPaths.push(dir);
	return dir;
};

let nacpPath = new URL('nxjs.nacp', root);
let exefsDir = new URL('exefs/', root);
let baseRomfsDir = new URL('romfs/', root);
let baseIconPath = new URL(iconName, root);
let iconPath = new URL(iconName, appRoot);
const logoDir = new URL('logo/', appRoot);
const romfsDir = new URL('romfs/', appRoot);
const nspDir = createTmpDir('nsp');
const ncaDir = createTmpDir('nca');
const tempDir = createTmpDir('temp');
const backupDir = createTmpDir('backup');
const controlDir = createTmpDir('control');

if (isSrcMode) {
	nacpPath = new URL('../../nxjs.nacp', root);
	baseIconPath = new URL(`../../${iconName}`, root);
	exefsDir = new URL('../../build/exefs/', root);
	baseRomfsDir = new URL('../../romfs/', root);
}

try {
	// Icon
	console.log(chalk.bold('Icon:'));
	if (!existsSync(iconPath)) {
		iconPath = baseIconPath;
		console.log(
			`⚠️  No ${chalk.bold(
				`"${iconName}"`
			)} file found. Default nx.js icon will be used.`
		);
	}
	const iconDest = new URL('icon_AmericanEnglish.dat', controlDir);
	const logo = await Jimp.read(fileURLToPath(iconPath));
	const logoWidth = logo.getWidth();
	const logoHeight = logo.getHeight();
	if (logoWidth !== 256 && logoHeight !== 256) {
		logo.resize(256, 256);
	}
	const logoBuf = await logo.getBufferAsync(Jimp.MIME_JPEG);
	writeFileSync(iconDest, logoBuf);
	console.log(await terminalImage.buffer(logoBuf, { height: 12 }));

	// NACP
	const nacp = new NACP(readFileSync(nacpPath).buffer);

	// Attempt to set a sensible default for `startupUserAccount`.
	// If `localStorage` is used in the app's "main.js" file, then
	// the profile selector should be shown.
	const mainPath = new URL('main.js', romfsDir);
	try {
		const main = readFileSync(mainPath, 'utf8');
		nacp.startupUserAccount = /\blocalStorage\b/.test(main) ? 1 : 0;
	} catch (err: any) {
		// If "main.js" does not exist, then the app will fail to
		// run anyways, so error out now.
		if (err.code === 'ENOENT') {
			throw new Error('No "main.js" file found in `romfs` directory');
		}
		throw err;
	}

	patchNACP(nacp, new URL('package.json', appRoot));
	writeFileSync(
		new URL('control.nacp', controlDir),
		Buffer.from(nacp.buffer)
	);
	const titleid = nacp.id.toString(16).padStart(16, '0');
	console.log();
	console.log(chalk.bold('Setting metadata:'));
	console.log(`  ID: ${chalk.green(titleid)}`);
	console.log(`  Title: ${chalk.green(nacp.title)}`);
	console.log(`  Version: ${chalk.green(nacp.version)}`);
	console.log(`  Author: ${chalk.green(nacp.author)}`);

	// RomFS
	for (const file of readdirSync(baseRomfsDir)) {
		const src = new URL(file, baseRomfsDir);
		const dest = new URL(file, romfsDir);
		cpSync(src, dest);
		tmpPaths.push(dest);
	}

	// Generate NSP file via `hacbrewpack`
	console.log();
	console.log(chalk.bold('Running `hacbrewpack`:'));

	const argv = [
		'--nspdir',
		fileURLToPath(nspDir),
		'--ncadir',
		fileURLToPath(ncaDir),
		'--tempdir',
		fileURLToPath(tempDir),
		'--backupdir',
		fileURLToPath(backupDir),
		'--exefsdir',
		fileURLToPath(exefsDir),
		'--romfsdir',
		fileURLToPath(romfsDir),
		'--controldir',
		fileURLToPath(controlDir),
		'--titleid',
		titleid,
		'--nopatchnacplogo',
		...(existsSync(logoDir) ? [] : ['--nologo']),
		...process.argv.slice(2),
	];

	const child = spawn('hacbrewpack', argv, { stdio: 'inherit', shell: true });
	const [exitCode, signal] = await once(child, 'exit');
	if (signal) {
		process.kill(process.pid, signal);
	} else if (exitCode) {
		process.exitCode = exitCode;
	} else {
		// move NSP file
		const nspFileName = readdirSync(nspDir)[0];
		const nspSrc = new URL(nspFileName, nspDir);
		const outputNspName = `${nacp.title}.nsp`;
		const nspDest = new URL(outputNspName, appRoot);
		cpSync(nspSrc, nspDest);
		console.log(
			chalk.green(
				`\n🎉 Success! Generated NSP file ${chalk.bold(
					`"${outputNspName}"`
				)}`
			) + ` (${bytes(statSync(nspDest).size).toLowerCase()})`
		);
	}
} catch (err: any) {
	process.exitCode = 1;
	console.log(chalk.red(`${chalk.bold('Error!')} ${err.message}`));
} finally {
	for (const dir of tmpPaths) {
		rmSync(dir, { recursive: true, force: true });
	}
}
