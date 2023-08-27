import fs from 'fs';
import { generateDtsBundle } from 'dts-bundle-generator';

const distDir = new URL('dist/', import.meta.url);

fs.mkdirSync(distDir, { recursive: true });

let [output] = generateDtsBundle(
	[
		{
			filePath: './src/index.ts',
		},
	],
	{
		noBanner: true,
	}
);

// We need to do some post-processing on the `dts-bundle-generator`
// output to make the result suitable as a runtime `.d.ts` file.

// 1) Add references to core JavaScript interfaces
// that are compatible with QuickJS.
output = `
/// <reference no-default-lib="true"/>

/// <reference lib="es2019" />
/// <reference lib="es2020.bigint" />
/// <reference lib="es2020.date" />
/// <reference lib="es2020.number" />
/// <reference lib="es2020.promise" />
/// <reference lib="es2020.sharedmemory" />
/// <reference lib="es2020.string" />
/// <reference lib="es2020.symbol.wellknown" />

${output}`;

// 2) Remove all `export` declarations.
output = output.replace(/^export /gm, '');

// 3) Remove all `implements globalThis.` statements.
output = output.replace(/\bimplements (.*){/g, (_, matches) => {
	const filtered = matches
		.split(',')
		.map(i => i.trim())
		.filter((i) => !i.startsWith('globalThis.'));
	if (filtered.length > 0) {
		return `implements ${filtered.join(', ')} {`;
	}
	return '{'
});

// 4) `ctx.canvas` is marked as `HTMLCanvasElement` to make TypeScript
// happy, but the class name in nx.js is `Canvas`. So let's fix that.
output = output.replace(/\bHTMLCanvasElement\b/g, 'Canvas');

// 5) At this point, the final line contains a `{};` which doesn't
// hurt, but also isn't necessary. Clean that up.
output = output.trim().split('\n').slice(0, -1).join('\n');

fs.writeFileSync(new URL('index.d.ts', distDir), output);