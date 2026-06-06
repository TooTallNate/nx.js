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
import { NACP } from '@tootallnate/nacp';
import { patchNACP } from '@nx.js/patch-nacp';
import * as NRO from '@tootallnate/nro';
import * as RomFS from '@tootallnate/romfs';
import terminalImage from 'terminal-image';

const cwd = process.cwd();
const appRoot = new URL(`${pathToFileURL(cwd)}/`);
const isSrcMode = existsSync(new URL('../tsconfig.json', import.meta.url));

// Slim mode: instead of embedding the full ~40 MB runtime, use the tiny
// `bootstrap.nro` launcher as the base. The launcher chainloads a shared
// runtime from `sdmc:/nx.js/` at boot. Enabled via `--slim` or `NXJS_SLIM=1`.
const slim =
	process.argv.slice(2).includes('--slim') || process.env.NXJS_SLIM === '1';

// The base NRO whose code segments we reuse: the bootstrap launcher (slim) or
// the full runtime (fat).
const baseName = slim ? 'bootstrap.nro' : 'nxjs.nro';
const baseNroUrl = new URL(
	isSrcMode
		? slim
			? '../../../bootstrap/bootstrap.nro'
			: '../../../nxjs.nro'
		: `../dist/${baseName}`,
	import.meta.url,
);
const nxjsNroBuffer = readFileSync(baseNroUrl);
const nxjsNroBlob = new Blob([nxjsNroBuffer]);
const nxjsNro = await NRO.decode(nxjsNroBlob);

if (slim) {
	console.log(
		chalk.bold(
			`Building a ${chalk.cyan('slim')} NRO (shared runtime via bootstrap launcher).`,
		),
	);
	console.log(
		chalk.dim(
			'  Requires an nx.js runtime NRO installed at sdmc:/nx.js/nxjs-v<version>.nro\n',
		),
	);
}

// Icon
let icon = nxjsNro.icon;
console.log(chalk.bold('Icon:'));
const iconName = 'icon.jpg';
try {
	const iconUrl = new URL(iconName, appRoot);
	const iconData = readFileSync(iconUrl);
	icon = new Blob([iconData]);
} catch (err: any) {
	if (err.code !== 'ENOENT') {
		throw err;
	}
	console.log(
		chalk.yellow(
			`⚠️  No ${chalk.bold(
				`"${iconName}"`,
			)} file found. Default nx.js icon will be used.`,
		),
	);
}
const logoBuf = Buffer.from(await icon!.arrayBuffer());
console.log(`  JPEG size: ${bytes(logoBuf.length)!.toLowerCase()}`);
console.log(await terminalImage.buffer(logoBuf, { height: 18 }));

// NACP
const originalNacp = new NACP(await nxjsNro.nacp!.arrayBuffer());
const nacp = new NACP(originalNacp.buffer.slice(0));
const { packageJson, updated, warnings } = patchNACP(
	nacp,
	new URL('package.json', appRoot),
);
console.log();
console.log(chalk.bold('NACP Metadata:'));
for (const warning of warnings) {
	console.log(chalk.yellow(`⚠️  ${warning}`));
}
for (const [k, v] of updated) {
	console.log(`  ${chalk.green(k)}: ${v}`);
}

// RomFS
const romfsDir = new URL('romfs/', appRoot);
const romfsDirPath = fileURLToPath(romfsDir);
// Fat base (nxjs.nro) ships a RomFS (runtime.js.map, GeistMono.ttf) to merge
// the app's files into. The slim base (bootstrap.nro) has no RomFS, so start
// from an empty tree.
const romfs: RomFS.RomFsEntry = nxjsNro.romfs
	? await RomFS.decode(nxjsNro.romfs)
	: Object.create(null);
console.log();
console.log(chalk.bold('RomFS Files:'));

function walk(dir: URL, dirEntry: RomFS.RomFsEntry) {
	for (const name of readdirSync(dir)) {
		const fileUrl = new URL(name, dir);
		const stat = statSync(fileUrl);
		if (stat.isDirectory()) {
			const entry = dirEntry[name] ?? Object.create(null);
			if (entry instanceof Blob) {
				throw new Error(`Expected directory: ${fileUrl}`);
			}
			dirEntry[name] = entry;
			walk(new URL(`${fileUrl}/`), entry);
		} else if (stat.isFile()) {
			const blob = new Blob([readFileSync(fileUrl)]);
			console.log(
				`  ${chalk.cyan(
					relative(romfsDirPath, fileURLToPath(fileUrl)),
				)} (${bytes(blob.size)!.toLowerCase()})`,
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

// Slim apps need an `nxjs.ini` with a `[runtime] version` semver requirement in
// their RomFS — the bootstrap launcher reads it to pick which shared runtime to
// chainload. If the app didn't provide one (no `romfs/nxjs.ini` with a
// [runtime] section), write a default caret-on-major requirement derived from
// this @nx.js/nro package's version (which tracks the runtime).
if (slim) {
	const existingIni = romfs['nxjs.ini'];
	const existingIniText =
		existingIni instanceof Blob ? await existingIni.text() : '';
	const hasRuntimeSection = /^\s*\[runtime\]/im.test(existingIniText);
	if (!hasRuntimeSection) {
		const selfPkg = JSON.parse(
			readFileSync(new URL('../package.json', import.meta.url), 'utf8'),
		);
		const major = String(selfPkg.version).replace(/[.-].*/, '');
		const runtimeReq = `^${major}`;
		// Prepend a [runtime] section, preserving any existing ini content.
		const merged =
			`[runtime]\n; nx.js shared runtime version requirement (semver).\nversion = ${runtimeReq}\n` +
			(existingIniText ? `\n${existingIniText}` : '');
		romfs['nxjs.ini'] = new Blob([merged]);
		console.log(
			`  ${chalk.cyan('nxjs.ini')} (generated; [runtime] version = ${runtimeReq})`,
		);
	}
}

const outputNroName = `${packageJson.name}.nro`;

if (!(romfs['main.js'] instanceof Blob)) {
	console.log();
	console.log(
		chalk.yellow(
			`${chalk.bold(
				'Warning!',
			)} No "main.js" file found in \`romfs\` directory.`,
		),
	);
	console.log(
		chalk.yellow(
			`The entrypoint file ${chalk.bold(
				`"${packageJson.name}.js"`,
			)} will need to`,
		),
	);
	console.log(
		chalk.yellow(
			`be placed alongside ${chalk.bold(`"${outputNroName}"`)} on the SD card.`,
		),
	);
}

// Generate final NRO file
const outputNro = await NRO.encode({
	...nxjsNro,
	icon,
	nacp: new Blob([new Uint8Array(nacp.buffer)]),
	romfs: await RomFS.encode(romfs),
});
const outputNroUrl = new URL(outputNroName, appRoot);
writeFileSync(outputNroUrl, Buffer.from(await outputNro.arrayBuffer()));
console.log(
	`${chalk.green(
		`\n🎉 Success! Generated NRO file ${chalk.bold(`"${outputNroName}"`)}`,
	)} (${bytes(outputNro.size)!.toLowerCase()})`,
);
