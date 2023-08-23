import fs from 'fs';

const appsDir = new URL('apps/', import.meta.url);

for (const app of fs.readdirSync(appsDir)) {
    const appUrl = new URL(`${app}/`, appsDir);
    // … do your thing…
}
