#!/usr/bin/env node
import { Blob } from 'node:buffer';
import { readFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import parseAuthor from 'parse-author';
import { extractNACP, extractRomFs } from '@tootallnate/nro';
import { parseHeader } from '@tootallnate/romfs';
import type { PackageJson } from 'types-package-json';

const packageRoot = new URL(`${pathToFileURL(process.cwd())}/`);

// Read the `package.json` file to get the information that will
// be embedded in the NACP section of the output `.nro` file
const packageJsonUrl = new URL('package.json', packageRoot);
const packageJsonStr = readFileSync(packageJsonUrl, 'utf8');
const packageJson: PackageJson = JSON.parse(packageJsonStr);
const { name, version, author: rawAuthor } = packageJson;
const author =
	typeof rawAuthor === 'string' ? parseAuthor(rawAuthor) : rawAuthor;
console.log({ name, version, author: author?.name });

const nxjsNroUrl = new URL('../dist/nxjs.nro', import.meta.url);
const nxjsNroBuffer = readFileSync(nxjsNroUrl);
const nxjsNroBlob = new Blob([nxjsNroBuffer]) as unknown as globalThis.Blob;
const nacp = await extractNACP(nxjsNroBlob);
console.log(nacp.title)
console.log(nacp.version)
console.log(nacp.author)
nacp.title = name;
if (version) {
    nacp.version = version;
}
if (author?.name) {
    nacp.author = author.name;
}
console.log(nacp.title)
console.log(nacp.version)
console.log(nacp.author)

const romfsDir = new URL('romfs', packageRoot);
const romfs = await extractRomFs(nxjsNroBlob);
console.log(romfs);
console.log(await parseHeader(romfs));
