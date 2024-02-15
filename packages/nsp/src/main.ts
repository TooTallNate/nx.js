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
import { NACP } from '@tootallnate/nacp';
import { patchNACP } from '@nx.js/patch-nacp';
import terminalImage from 'terminal-image';

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
let iconPath = new URL('icon.jpg', root);
let exefsDir = new URL('exefs/', root);
let baseRomfsDir = new URL('romfs/', root);
const romfsDir = new URL('romfs/', appRoot);
const nspDir = createTmpDir('nsp');
const ncaDir = createTmpDir('nca');
const tempDir = createTmpDir('temp');
const backupDir = createTmpDir('backup');
let logoDir = new URL('logo/', appRoot);
if (!existsSync(logoDir)) {
	logoDir = createTmpDir('logo');
}
const controlDir = createTmpDir('control');

if (isSrcMode) {
	nacpPath = new URL('../../nxjs.nacp', root);
	iconPath = new URL('../../icon.jpg', root);
	exefsDir = new URL('../../build/exefs/', root);
	baseRomfsDir = new URL('../../romfs/', root);
}

try {
	// Icon
	cpSync(iconPath, new URL('icon_AmericanEnglish.dat', controlDir));
	console.log(chalk.bold('Icon:'));
	console.log(
		await terminalImage.file(fileURLToPath(iconPath), { height: 12 })
	);

	// NACP
	const nacp = new NACP(readFileSync(nacpPath).buffer);
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

	// TODO: validate `romfs/main.js` exists

	// Generate NSP file via `hacbrewpack`
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
		'--logodir',
		fileURLToPath(logoDir),
		'--controldir',
		fileURLToPath(controlDir),
		'--titleid',
		titleid,
		'--nopatchnacplogo',
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
} finally {
	for (const dir of tmpPaths) {
		rmSync(dir, { recursive: true, force: true });
	}
}
