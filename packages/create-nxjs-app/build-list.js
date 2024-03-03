import fs from 'fs';

const distDir = new URL('dist/', import.meta.url);
const appsDir = new URL('../../apps/', import.meta.url);
const packagesDir = new URL('../../packages/', import.meta.url);

const appMeta = [];
for (const app of fs.readdirSync(appsDir)) {
	if (app === 'tests') continue;
	const appDir = new URL(app, appsDir);
	if (!fs.lstatSync(appDir).isDirectory()) continue;
	const pkg = JSON.parse(
		fs.readFileSync(new URL(`${app}/package.json`, appsDir), 'utf8'),
	);
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
		},
		null,
		2,
	),
);
