import { URL } from './polyfills/url';
import { dataUriToBuffer } from 'data-uri-to-buffer';
import { decoder } from './polyfills/text-decoder';
import {
	TraceMap,
	originalPositionFor,
	type EncodedSourceMap,
} from '@jridgewell/trace-mapping';
import { readFileSync } from './fs';

interface CallSite {
	/**
	 * Is this call in native (V8 / C++) code?
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

	// `null` means the source map could not be retrieved for this file.
	tracer = null;

	try {
		// `filename` may not be a valid absolute URL (e.g. V8 reports frames
		// with synthetic scheme names like `app:...` for bundled code). In that
		// case URL construction throws — treat it as "no source map" rather than
		// letting the throw escape and mask the real error being formatted.
		let base: string | undefined = filename;
		try {
			void new URL(filename);
		} catch {
			base = undefined;
		}

		let sourceMapBuffer: ArrayBuffer | null | undefined;
		let sourceMappingURL: string | URL;
		if (base) {
			// Check for the `.map` file based on the filename as a shortcut.
			sourceMappingURL = new URL(`${filename}.map`, base);
			sourceMapBuffer = readFileSync(sourceMappingURL);
		}

		if (!sourceMapBuffer && base) {
			// When the `.map` file is not found, try the `sourceMappingURL`
			// embedded in the source (data URI or non-standard location).
			const contentsBuffer = readFileSync(filename);
			if (contentsBuffer) {
				const contents = decoder.decode(contentsBuffer).trimEnd();
				const lastNewline = contents.lastIndexOf('\n');
				const lastLine = contents.slice(lastNewline + 1);
				if (lastLine.startsWith(SOURCE_MAPPING_URL_PREFIX)) {
					const url = lastLine.slice(SOURCE_MAPPING_URL_PREFIX.length);
					if (url.startsWith('data:')) {
						sourceMapBuffer = dataUriToBuffer(url).buffer;
					} else {
						sourceMapBuffer = readFileSync(new URL(url, base));
					}
				}
			}
		}

		if (sourceMapBuffer) {
			const sourceMap: EncodedSourceMap = JSON.parse(
				decoder.decode(sourceMapBuffer),
			);
			tracer = new TraceMap(sourceMap);
		}
	} catch {
		// Any failure resolving/parsing the source map: fall back to no map.
		tracer = null;
	}

	sourceMapCache.set(filename, tracer);
	return tracer;
}

(Error as any).prepareStackTrace = (_: Error, callsites: CallSite[]) => {
	return callsites
		.map((callsite) => {
			try {
				let loc = callsite.isNative() ? 'native' : 'unknown';
				let name = callsite.getFunctionName() || '<anonymous>';
				let filename = callsite.getFileName();
				// V8 uses `<anonymous>` for eval/REPL frames (QuickJS used
				// `<input>`); neither has a source map to resolve.
				if (filename === '<anonymous>' || filename === '<input>') {
					return `    at ${filename}:${callsite.getLineNumber()}:${callsite.getColumnNumber()}`;
				}
				if (filename) {
					const proto = filename === 'romfs:/runtime.js' ? 'nxjs' : 'app';
					let line = callsite.getLineNumber() ?? 1;
					let column = callsite.getColumnNumber() ?? 1;

					const tracer = filenameToTracer(filename);
					if (tracer) {
						const traced = originalPositionFor(tracer, {
							line,
							column,
						});
						if (typeof traced.source === 'string') filename = traced.source;
						if (typeof traced.name === 'string') name = traced.name;
						if (typeof traced.column === 'number') column = traced.column;
						if (typeof traced.line === 'number') line = traced.line;
					}

					loc = `${proto}:${filename}:${line}:${column}`;
				}
				return `    at ${name} (${loc})`;
			} catch (err: unknown) {
				return `    <error calculating stack: ${err}>`;
			}
		})
		.join('\n');
};
