import { loader } from 'fumadocs-core/source';
import {
	clkrstDocs,
	constantsDocs,
	httpDocs,
	inspectDocs,
	ncmDocs,
	replDocs,
	runtimeDocs,
	utilDocs,
} from '../.source/server';

export const loaders = new Map([
	[
		'clkrst',
		loader({
			baseUrl: '/clkrst',
			source: clkrstDocs.toFumadocsSource(),
		}),
	],
	[
		'constants',
		loader({
			baseUrl: '/constants',
			source: constantsDocs.toFumadocsSource(),
		}),
	],
	[
		'http',
		loader({
			baseUrl: '/http',
			source: httpDocs.toFumadocsSource(),
		}),
	],
	[
		'inspect',
		loader({
			baseUrl: '/inspect',
			source: inspectDocs.toFumadocsSource(),
		}),
	],
	[
		'ncm',
		loader({
			baseUrl: '/ncm',
			source: ncmDocs.toFumadocsSource(),
		}),
	],
	[
		'repl',
		loader({
			baseUrl: '/repl',
			source: replDocs.toFumadocsSource(),
		}),
	],
	[
		'runtime',
		loader({
			baseUrl: '/runtime',
			source: runtimeDocs.toFumadocsSource(),
		}),
	],
	[
		'util',
		loader({
			baseUrl: '/util',
			source: utilDocs.toFumadocsSource(),
		}),
	],
]);
