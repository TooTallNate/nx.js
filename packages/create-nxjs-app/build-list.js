import fs from 'fs';
import { parse as parseYaml } from 'yaml';

const distDir = new URL('dist/', import.meta.url);
const appsDir = new URL('../../apps/', import.meta.url);
const packagesDir = new URL('../../packages/', import.meta.url);
const workspaceYaml = new URL('../../pnpm-workspace.yaml', import.meta.url);

const appMeta = [];
for (const app of fs.readdirSync(appsDir)) {
	if (app === 'tests') continue;
	const appDir = new URL(app, appsDir);
	if (!fs.lstatSync(appDir).isDirectory()) continue;
	const pkgPath = new URL(`${app}/package.json`, appsDir);
	if (!fs.existsSync(pkgPath)) continue;
	const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
	appMeta.push({
		name: app,
		description: pkg.description,
	});
}

const packages = {};
for (const pkgName of fs.readdirSync(packagesDir)) {
	const pkg = JSON.parse(
		fs.readFileSync(new URL(`${pkgName}/package.json`, packagesDir), 'utf8'),
	);
	packages[pkg.name] = {
		version: pkg.version,
	};
}

// Read the pnpm catalog from pnpm-workspace.yaml
const workspace = parseYaml(fs.readFileSync(workspaceYaml, 'utf8'));
const catalog = workspace.catalog || {};

// make "hello-world" be the first entry
const helloWorldIndex = appMeta.findIndex((e) => e.name === 'hello-world');
const helloWorld = appMeta.splice(helloWorldIndex, 1)[0];
appMeta.unshift(helloWorld);

fs.mkdirSync(distDir, { recursive: true });
fs.writeFileSync(
	new URL('data.json', distDir),
	JSON.stringify(
		{
			apps: appMeta,
			packages,
			catalog,
		},
		null,
		2,
	),
);
