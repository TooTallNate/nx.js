import { join } from 'node:path';
import { once } from 'node:events';
import { spawn } from 'node:child_process';
import { tmpdir } from 'node:os';
import { pathToFileURL, fileURLToPath } from 'node:url';
import {
	cpSync,
	existsSync,
	rmSync,
	mkdtempSync,
	readFileSync,
	writeFileSync,
	readdirSync,
} from 'node:fs';
import { NACP } from '@tootallnate/nacp';

const tmpPaths: URL[] = [];
const tmpDir = tmpdir();
const cwd = process.cwd();
const root = new URL('../', import.meta.url);
const packageRoot = new URL(`${pathToFileURL(cwd)}/`);
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
const romfsDir = new URL('romfs/', packageRoot);
const nspDir = createTmpDir('nsp');
const ncaDir = createTmpDir('nca');
const tempDir = createTmpDir('temp');
const backupDir = createTmpDir('backup');
let logoDir = new URL('logo/', packageRoot);
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
	const nacp = new NACP(readFileSync(nacpPath).buffer);
	writeFileSync(
		new URL('control.nacp', controlDir),
		new Uint8Array(nacp.buffer)
	);
	cpSync(iconPath, new URL('icon_AmericanEnglish.dat', controlDir));

	for (const file of readdirSync(baseRomfsDir)) {
		const src = new URL(file, baseRomfsDir);
		const dest = new URL(file, romfsDir);
		cpSync(src, dest);
		tmpPaths.push(dest);
	}

	// TODO: validate `romfs/main.js` exists

	const titleid = nacp.id.toString(16).padStart(16, '0');

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
	console.log(argv);

	const child = spawn('hacbrewpack', argv, { stdio: 'inherit' });
	const [exitCode, signal] = await once(child, 'exit');
	if (signal) {
		process.kill(process.pid, signal);
	} else if (exitCode) {
		process.exitCode = exitCode;
	} else {
		// move NSP file
		const nspFileName = readdirSync(nspDir)[0];
		const nspSrc = new URL(nspFileName, nspDir);
		const nspDest = new URL(`${nacp.title}.nsp`, packageRoot);
		cpSync(nspSrc, nspDest);
	}
} finally {
	for (const dir of tmpPaths) {
		console.log(`deleting ${dir}`);
		rmSync(dir, { recursive: true, force: true });
	}
}
