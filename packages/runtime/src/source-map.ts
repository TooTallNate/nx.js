import { dataUriToBuffer } from 'data-uri-to-buffer';
import {
	TraceMap,
	originalPositionFor,
	type EncodedSourceMap,
} from '@jridgewell/trace-mapping';
import { readFileSync } from './fs';

interface CallSite {
	/**
	 * Is this call in native quickjs code?
	 */
	isNative(): boolean;

	/**
	 * Name of the script [if this function was defined in a script]
	 */
	getFileName(): string | undefined;

	/**
	 * Current function
	 */
	getFunction(): Function | undefined;

	/**
	 * Name of the current function, typically its name property.
	 * If a name property is not available an attempt will be made to try
	 * to infer a name from the function's context.
	 */
	getFunctionName(): string | null;

	/**
	 * Current column number [if this function was defined in a script]
	 */
	getColumnNumber(): number | null;

	/**
	 * Current line number [if this function was defined in a script]
	 */
	getLineNumber(): number | null;
}

const SOURCE_MAPPING_URL_PREFIX = '//# sourceMappingURL=';
const sourceMapCache = new Map<string, TraceMap | null>();

function filenameToTracer(filename: string) {
	let tracer = sourceMapCache.get(filename);
	if (typeof tracer !== 'undefined') return tracer;

	// `null` means the source map could not be retrieved for this file
	tracer = null;

	const contentsBuffer = readFileSync(filename);
	const contents = new TextDecoder().decode(contentsBuffer).trimEnd();
	const lastNewline = contents.lastIndexOf('\n');
	const lastLine = contents.slice(lastNewline + 1);
	if (lastLine.startsWith(SOURCE_MAPPING_URL_PREFIX)) {
		const sourceMappingURL = lastLine.slice(
			SOURCE_MAPPING_URL_PREFIX.length
		);
		let sourceMapBuffer: ArrayBuffer;
		if (sourceMappingURL.startsWith('data:')) {
			sourceMapBuffer = dataUriToBuffer(sourceMappingURL).buffer;
		} else {
			sourceMapBuffer = readFileSync(new URL(sourceMappingURL, filename));
		}
		const sourceMap: EncodedSourceMap = JSON.parse(
			new TextDecoder().decode(sourceMapBuffer)
		);
		tracer = new TraceMap(sourceMap);
	}
	sourceMapCache.set(filename, tracer);
	return tracer;
}

(Error as any).prepareStackTrace = (_: Error, callsites: CallSite[]) => {
	return callsites
		.map((callsite) => {
			try {
				let loc = 'native';
				let name = callsite.getFunctionName() || '<anonymous>';
				let filename = callsite.getFileName();
				if (filename) {
					const proto =
						filename === 'romfs:/runtime.js' ? 'nxjs' : 'app';
					let line = callsite.getLineNumber() ?? 1;
					let column = callsite.getColumnNumber() ?? 1;

					const tracer = filenameToTracer(filename);
					if (tracer) {
						const traced = originalPositionFor(tracer, {
							line,
							column,
						});
						if (typeof traced.source === 'string')
							filename = traced.source;
						if (typeof traced.name === 'string') name = traced.name;
						if (typeof traced.column === 'number')
							column = traced.column;
						if (typeof traced.line === 'number') line = traced.line;
					}

					loc = `${proto}:${filename}:${line}:${column}`;
				}
				return `    at ${name} (${loc})`;
			} catch (_) {}
			return '???3';
		})
		.join('\n');
};
