#!/usr/bin/env node
import { readFileSync } from 'fs';
import { pathToFileURL } from 'url';
import parseAuthor from 'parse-author';
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
const nxjsNro = readFileSync(nxjsNroUrl);

const romfsDir = new URL('romfs', packageRoot);
