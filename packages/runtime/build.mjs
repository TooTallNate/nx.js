import fs from 'fs';
import ts from 'typescript';
import { generateDtsBundle } from 'dts-bundle-generator';

const distDir = new URL('dist/', import.meta.url);

fs.mkdirSync(distDir, { recursive: true });

let [globalTypes, switchTypes, wasmTypes] = generateDtsBundle(
	[
		{
			filePath: './src/index.ts',
		},
		{
			filePath: './src/switch.ts',
		},
		{
			filePath: './src/wasm.ts',
		},
	],
	{
		preferredConfigPath: 'tsconfig.json',
	}
);

const globalNames = new Set();

function transform(input, opts = {}) {
	// Remove banner
	input = input.split('\n').slice(2).join('\n');

	const sourceFile = ts.createSourceFile(
        'file.d.ts',
        input,
        ts.ScriptTarget.Latest,
        true,
        ts.ScriptKind.TS
    );

	function visit(node, context) {
		// For class/interface/type, remove if part of a namespace
		// where the type has already been defined in the global scope
		if (
			ts.isClassDeclaration(node) ||
			ts.isInterfaceDeclaration(node) ||
			ts.isTypeAliasDeclaration(node)
		) {
			const name = node.name?.getText();
			if (name) {
				if (opts.removeTypes) {
					if (opts.removeTypes.has(name)) return undefined;
				} else {
					globalNames.add(name);
				}
			}
		}

		// Check for and remove stray 'export {}'
		if (ts.isExportDeclaration(node)) {
			if (
				!node.exportClause ||
				(ts.isNamedExports(node.exportClause) &&
					node.exportClause.elements.length === 0)
			) {
				return undefined; // Remove this node
			}
		}

		if (ts.isClassDeclaration(node)) {
			// Filter or remove 'implements globalThis.*' from heritageClauses
			const newHeritageClauses = node.heritageClauses
				?.filter((clause) => {
					if (clause.token === ts.SyntaxKind.ImplementsKeyword) {
						const newTypes = clause.types.filter((type) => {
							return !type.expression
								.getText(sourceFile)
								.startsWith('globalThis.');
						});
						return newTypes.length > 0; // Keep the clause only if there are types left after filtering
					}
					return true; // Keep 'extends' and other clauses
				})
				.map((clause) => {
					// Update the clause with the filtered types
					if (clause.token === ts.SyntaxKind.ImplementsKeyword) {
						const newTypes = clause.types.filter((type) => {
							return !type.expression
								.getText(sourceFile)
								.startsWith('globalThis.');
						});
						return ts.factory.updateHeritageClause(
							clause,
							newTypes
						);
					}
					return clause;
				});
			console.log({
				name: node.name?.getText(),
				t: newHeritageClauses,
			});

			node = ts.factory.updateClassDeclaration(
				node,
				node.modifiers,
				node.name,
				node.typeParameters,
				newHeritageClauses?.length > 0 ? newHeritageClauses : undefined,
				node.members
			);
		}

        // Process class, interface, type alias, and function declarations
        if (
			(ts.isClassDeclaration(node) ||
				ts.isInterfaceDeclaration(node) ||
				ts.isTypeAliasDeclaration(node) ||
				ts.isFunctionDeclaration(node) ||
				ts.isVariableStatement(node)) &&
			node.modifiers?.some(
				(modifier) => modifier.kind === ts.SyntaxKind.ExportKeyword
			)
		) {
			// Remove the 'export' modifier
			const newModifiers = node.modifiers.filter(
				(modifier) => modifier.kind !== ts.SyntaxKind.ExportKeyword
			);

			if (ts.isClassDeclaration(node)) {
				return ts.factory.updateClassDeclaration(
					node,
					newModifiers,
					node.name,
					node.typeParameters,
					node.heritageClauses,
					node.members
				);
			} else if (ts.isInterfaceDeclaration(node)) {
				return ts.factory.updateInterfaceDeclaration(
					node,
					newModifiers,
					node.name,
					node.typeParameters,
					node.heritageClauses,
					node.members
				);
			} else if (ts.isTypeAliasDeclaration(node)) {
				return ts.factory.updateTypeAliasDeclaration(
					node,
					newModifiers,
					node.name,
					node.typeParameters,
					node.type
				);
			} else if (ts.isFunctionDeclaration(node)) {
				return ts.factory.updateFunctionDeclaration(
					node,
					newModifiers,
					node.asteriskToken,
					node.name,
					node.typeParameters,
					node.parameters,
					node.type,
					node.body
				);
			} else if (ts.isVariableStatement(node)) {
				return ts.factory.updateVariableStatement(
                    node,
                    newModifiers,
                    node.declarationList
                );
			}
		}

		return ts.visitEachChild(
			node,
			(child) => visit(child, context),
			context
		);
	}

    const result = ts.transform(sourceFile, [context => rootNode => ts.visitNode(rootNode, node => visit(node, context))]);
    return ts.createPrinter().printFile(result.transformed[0]);
}

function namespace(name, input) {
	return `declare namespace ${name} {
${transform(input, { removeTypes: globalNames })
	.split('\n')
	.map((l) => `\t${l}`)
	.join('\n')}
}`;
}

const output = `/// <reference no-default-lib="true"/>
/// <reference lib="es2022" />

${transform(globalTypes)}
`;
//${namespace('Switch', switchTypes)}
//${namespace('WASM', wasmTypes)}
console.log(globalNames);

fs.writeFileSync(new URL('index.d.ts', distDir), output);
