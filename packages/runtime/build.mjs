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

/// <reference lib="es2022" />

${output}`;

// 2) Remove all `export` declarations.
output = output.replace(/^export /gm, '');

function splitOnComma(str) {
    let result = [];
    let depth = 0;
    let start = 0;

    for (let i = 0; i < str.length; i++) {
        switch (str[i]) {
            case '<':
                depth++;
                break;
            case '>':
                depth--;
                break;
            case ',':
                if (depth === 0) {
                    result.push(str.slice(start, i).trim());
                    start = i + 1;
                }
                break;
        }
    }

    result.push(str.slice(start).trim()); // Add the last segment

    return result;
}

// 3) Remove all `implements globalThis.` statements.
output = output.replace(/\bimplements (.*){/g, (_, matches) => {
	const filtered = splitOnComma(matches).filter(
		(i) => !i.includes('globalThis.')
	);
	if (filtered.length > 0) {
		return `implements ${filtered.join(', ')} {`;
	}
	return '{';
});

// 4) `ctx.canvas` is marked as `HTMLCanvasElement` to make TypeScript
// happy, but the class name in nx.js is `Canvas`. So let's fix that.
output = output.replace(/\bHTMLCanvasElement\b/g, 'Canvas');

// 5) At this point, the final line contains a `{};` which doesn't
// hurt, but also isn't necessary. Clean that up.
output = output.trim().split('\n').slice(0, -1).join('\n');

function generateNamespace(name, filePath) {
	let [output] = generateDtsBundle(
		[
			{
				filePath,
			},
		],
		{
			noBanner: true,
		}
	);
	output = output.replace(/^export (declare )?/gm, '');
	output = output.replace(/\bimplements (.*){/g, (_, matches) => {
		const filtered = matches
			.split(',')
			.map((i) => i.trim())
			.filter((i) => !i.startsWith(`${name}.`));
		if (filtered.length > 0) {
			return `implements ${filtered.join(', ')} {`;
		}
		return '{';
	});
	output = output.trim().split('\n').slice(2, -2).map(l => `  ${l}`).join('\n');
	//console.log({ output });
	return `declare namespace ${name} {\n${output}\n}\n`;
}

// `dts-bundle-generator` does not currently support namespaces,
// so we need to add those in manually:
// See: https://github.com/timocov/dts-bundle-generator/issues/134
output += generateNamespace('WebAssembly', './src/wasm.ts');
output += generateNamespace('Switch', './src/switch.ts');

fs.writeFileSync(new URL('index.d.ts', distDir), output);
