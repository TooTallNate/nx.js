import fs from 'fs';

const distDir = new URL('dist/', import.meta.url);
const appsDir = new URL('../../apps/', import.meta.url);
const apps = fs.readdirSync(appsDir);

const json = [];

for (const app of apps) {
    if (app === 'tests') continue;
    const appDir = new URL(app, appsDir);
    if (!fs.lstatSync(appDir).isDirectory()) continue;
    const pkg = JSON.parse(fs.readFileSync(new URL(`${app}/package.json`, appsDir), 'utf8'));
    json.push({
        name: app,
        description: pkg.description
    });
}

fs.mkdirSync(distDir, { recursive: true });
fs.writeFileSync(
	new URL('choices.json', distDir),
	JSON.stringify(json, null, 2)
);