#!/usr/bin/env node
import bytes from 'bytes';
import chalk from 'chalk';
import {
	cpSync,
	existsSync,
	readFileSync,
	writeFileSync,
	readdirSync,
	statSync,
	rmSync,
} from 'node:fs';
import { join } from 'node:path';
import { homedir } from 'node:os';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { Jimp } from 'jimp';
import { NACP } from '@tootallnate/nacp';
import { patchNACP } from '@nx.js/patch-nacp';
import { buildNsp, type RomFsEntry } from '@tootallnate/hacbrewpack';
import terminalImage from 'terminal-image';

const iconName = 'icon.jpg';
const cwd = process.cwd();
const root = new URL('../', import.meta.url);
const appRoot = new URL(`${pathToFileURL(cwd)}/`);
const isSrcMode = existsSync(new URL('tsconfig.json', root));

// Parse CLI arguments
let keysetPath: string | undefined;
let plaintext = false;
let fat = process.env.NXJS_FAT === '1';
const args = process.argv.slice(2);
for (let i = 0; i < args.length; i++) {
	if (args[i] === '-k' || args[i] === '--keyset') {
		keysetPath = args[++i];
	} else if (args[i] === '--plaintext') {
		plaintext = true;
	} else if (args[i] === '--fat') {
		fat = true;
	}
	// `--slim` / NXJS_SLIM are accepted as no-ops (slim is the default).
}

// Packaging mode. SLIM is the default: the NSP's exefs `main` is a tiny
// forwarder (a patched nx-hbloader) that chainloads the shared runtime NRO from
// sdmc:/nx.js/ and mounts this title's RomFS (the app) into it. `--fat`
// (or NXJS_FAT=1) embeds the full runtime NSO as `main` (self-contained).
const slim = !fat;

let nacpPath = new URL('nxjs.nacp', root);
let exefsDir = new URL('exefs/', root);
let baseRomfsDir = new URL('romfs/', root);
let baseIconPath = new URL(iconName, root);
let iconPath = new URL(iconName, appRoot);
const logoDir = new URL('logo/', appRoot);
const htmlDocUserDir = new URL('htmldoc/', appRoot);
const romfsDir = new URL('romfs/', appRoot);

if (isSrcMode) {
	nacpPath = new URL('../../nxjs.nacp', root);
	baseIconPath = new URL(`../../${iconName}`, root);
	exefsDir = new URL('../../build/exefs/', root);
	baseRomfsDir = new URL('../../romfs/', root);
}

// Slim mode: the exefs `main` NSO + `main.npdm` come from the forwarder
// (bootstrap/launcher-nsp -> hbl.nso/hbl.npdm), not the full runtime exefs.
const forwarderNso = new URL(
	isSrcMode ? '../../bootstrap/launcher-nsp/hbl.nso' : 'dist/hbl.nso',
	root,
);
const forwarderNpdm = new URL(
	isSrcMode ? '../../bootstrap/launcher-nsp/hbl.npdm' : 'dist/hbl.npdm',
	root,
);

/**
 * Recursively read a directory into a RomFsEntry tree.
 */
function readDirToRomFsEntry(dirPath: string): RomFsEntry {
	const entry: RomFsEntry = {};
	for (const name of readdirSync(dirPath)) {
		const fullPath = join(dirPath, name);
		const stat = statSync(fullPath);
		if (stat.isDirectory()) {
			entry[name] = readDirToRomFsEntry(fullPath);
		} else {
			entry[name] = new Blob([readFileSync(fullPath)]);
		}
	}
	return entry;
}

/**
 * Find a key file following the same search order as hacbrewpack:
 * 1. Explicit path from -k / --keyset flag
 * 2. ./keys.dat
 * 3. ./keys.txt
 * 4. ./keys.ini
 * 5. ./prod.keys
 * 6. $HOME/.switch/prod.keys
 */
function findKeyFile(explicitPath?: string): string | null {
	if (explicitPath) {
		if (existsSync(explicitPath)) {
			return explicitPath;
		}
		return null;
	}
	const names = ['keys.dat', 'keys.txt', 'keys.ini', 'prod.keys'];
	for (const name of names) {
		const fullPath = join(cwd, name);
		if (existsSync(fullPath)) {
			return fullPath;
		}
	}
	// Try $HOME/.switch/prod.keys
	const home = homedir();
	if (home) {
		const fullPath = join(home, '.switch', 'prod.keys');
		if (existsSync(fullPath)) {
			return fullPath;
		}
	}
	return null;
}

// Track files copied into romfs for cleanup
const copiedRomfsFiles: string[] = [];

try {
	// Icon
	console.log(chalk.bold('Icon:'));
	if (!existsSync(iconPath)) {
		iconPath = baseIconPath;
		console.log(
			chalk.yellow(
				`⚠️  No ${chalk.bold(
					`"${iconName}"`,
				)} file found. Default nx.js icon will be used.`,
			),
		);
	}
	const logo = await Jimp.read(fileURLToPath(iconPath));
	if (logo.width !== 256 || logo.height !== 256) {
		logo.resize({ w: 256, h: 256 });
	}
	const logoBuf = await logo.getBuffer('image/jpeg');
	console.log(`  JPEG size: ${bytes(logoBuf.length)!.toLowerCase()}`);
	console.log(await terminalImage.buffer(logoBuf, { height: 18 }));

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

	const { packageJson, updated, warnings } = patchNACP(
		nacp,
		new URL('package.json', appRoot),
	);
	const titleid = nacp.id.toString(16).padStart(16, '0');
	console.log();
	console.log(chalk.bold('NACP Metadata:'));
	for (const warning of warnings) {
		console.log(chalk.yellow(`⚠️  ${warning}`));
	}
	for (const [k, v] of updated) {
		console.log(`  ${chalk.green(k)}: ${v}`);
	}

	// Fat: copy the base runtime RomFS files (runtime.js.map, GeistMono.ttf)
	// into the app's romfs — the embedded runtime serves them from `romfs:`.
	// Slim: skip — those belong to the shared runtime NRO (served as `nxjs:`),
	// not the app; the slim NSP's romfs holds ONLY the app's files + nxjs.ini.
	if (!slim) {
		for (const file of readdirSync(baseRomfsDir)) {
			const src = fileURLToPath(new URL(file, baseRomfsDir));
			const dest = join(fileURLToPath(romfsDir), file);
			cpSync(src, dest);
			copiedRomfsFiles.push(dest);
		}
	}



	// Read keys
	const keyFilePath = findKeyFile(keysetPath);
	if (!keyFilePath) {
		throw new Error(
			'Unable to find keyset file.\n' +
				'Use -k or --keyset to specify your keyset file path,\n' +
				'or place your keyset as "keys.dat", "keys.txt", "keys.ini", or "prod.keys" in the current directory,\n' +
				'or at "$HOME/.switch/prod.keys".',
		);
	}
	console.log();
	console.log(`Loading ${chalk.bold(`"${keyFilePath}"`)} keyset file`);
	const keysContent = readFileSync(keyFilePath, 'utf8');

	// Read ExeFS files. Slim: the forwarder NSO/NPDM (renamed to main/main.npdm).
	// Fat: the full runtime exefs (main = the ~21 MB runtime NSO).
	const exefsFiles = new Map<string, Uint8Array>();
	if (slim) {
		exefsFiles.set('main', readFileSync(forwarderNso));
		exefsFiles.set('main.npdm', readFileSync(forwarderNpdm));
	} else {
		for (const name of readdirSync(exefsDir)) {
			const filePath = fileURLToPath(new URL(name, exefsDir));
			if (statSync(filePath).isFile()) {
				exefsFiles.set(name, readFileSync(filePath));
			}
		}
	}

	// Read control files
	const controlFiles = new Map<string, Uint8Array>();
	controlFiles.set('control.nacp', new Uint8Array(nacp.buffer));
	controlFiles.set('icon_AmericanEnglish.dat', logoBuf);

	// Read RomFS directory
	const romfsEntry = readDirToRomFsEntry(fileURLToPath(romfsDir));

	// Slim apps need an `nxjs.ini` with a `[runtime] version` requirement so the
	// forwarder knows which shared runtime to load. If the app didn't provide a
	// [runtime] section, inject a default caret-on-major requirement (derived
	// from this @nx.js/nsp version). Injected into the in-memory romfs tree — no
	// mutation of the app's romfs/ dir on disk.
	if (slim) {
		const existingIni = romfsEntry['nxjs.ini'];
		const existingIniText =
			existingIni instanceof Blob ? await existingIni.text() : '';
		if (!/^\s*\[runtime\]/im.test(existingIniText)) {
			const selfPkg = JSON.parse(
				readFileSync(new URL('package.json', root), 'utf8'),
			);
			// Caret on the FULL version this builder (== the runtime version,
			// since the packages are version-locked) was published as — "at
			// least the runtime this app was packaged with, or any newer
			// compatible release". A caret-on-major (^1) would wrongly accept an
			// OLDER runtime missing features the app expects.
			const runtimeReq = `^${selfPkg.version}`;
			const merged =
				`[runtime]\n; nx.js shared runtime version requirement (semver).\nversion = ${runtimeReq}\n` +
				(existingIniText ? `\n${existingIniText}` : '');
			romfsEntry['nxjs.ini'] = new Blob([merged]);
			console.log(
				`  ${chalk.cyan('nxjs.ini')} (generated; [runtime] version = ${runtimeReq})`,
			);
		}
	}

	// Read Logo files (optional)
	let logoFiles: Map<string, Uint8Array> | undefined;
	const hasLogo = existsSync(logoDir);
	if (hasLogo) {
		logoFiles = new Map();
		for (const name of readdirSync(logoDir)) {
			const filePath = fileURLToPath(new URL(name, logoDir));
			if (statSync(filePath).isFile()) {
				logoFiles.set(name, readFileSync(filePath));
			}
		}
	}

	// Read HtmlDoc directory (optional)
	let htmldocEntry: RomFsEntry | undefined;
	if (existsSync(htmlDocUserDir)) {
		// Wrap htmldoc/ contents into html-document/.htdocs/ structure
		const innerEntry = readDirToRomFsEntry(fileURLToPath(htmlDocUserDir));
		htmldocEntry = {
			'html-document': {
				'.htdocs': innerEntry,
			},
		};
	}

	// Generate NSP
	console.log();
	console.log(
		chalk.bold(
			`Generating a ${chalk.cyan(slim ? 'slim' : 'fat')} NSP${
				slim
					? ' (shared runtime via forwarder; needs sdmc:/nx.js/nxjs-v<version>.nro)'
					: ' (self-contained; runtime embedded)'
			}:`,
		),
	);

	const startTime = performance.now();
	const result = await buildNsp({
		keys: keysContent,
		titleId: titleid,
		noPatchNacpLogo: true,
		noLogo: !hasLogo,
		plaintext,
		exefs: exefsFiles,
		control: controlFiles,
		romfs: romfsEntry,
		logo: logoFiles,
		htmldoc: htmldocEntry,
	});
	const elapsed = performance.now() - startTime;

	// Write the NSP file
	const outputNspName = `${packageJson.name}.nsp`;
	const nspDest = join(cwd, outputNspName);
	const nspBuffer = Buffer.from(await result.nsp.arrayBuffer());
	writeFileSync(nspDest, nspBuffer);

	const elapsedStr =
		elapsed < 1000
			? `${Math.round(elapsed)}ms`
			: `${(elapsed / 1000).toFixed(2)}s`;
	console.log();
	console.log(
		chalk.green(
			`🎉 Success! Generated NSP file ${chalk.bold(`"${outputNspName}"`)}`,
		) + ` (${bytes(nspBuffer.length)!.toLowerCase()}) in ${elapsedStr}`,
	);
} catch (err: any) {
	process.exitCode = 1;
	console.log(chalk.red(`${chalk.bold('Error!')} ${err.message}`));
} finally {
	// Clean up copied romfs files
	for (const file of copiedRomfsFiles) {
		rmSync(file, { force: true });
	}
}
