import { dataUriToBuffer } from 'data-uri-to-buffer';
import {
	TraceMap,
	originalPositionFor,
	type EncodedSourceMap,
} from '@jridgewell/trace-mapping';
import { readFileSync } from './fs';

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

(Error as any).prepareStackTrace = (_: Error, stack: string) => {
	return stack
		.split('\n')
		.map((line) => {
			try {
				const m = line.match(/(\s+at )(.*) \((.*)\:(\d+)\)$/);
				if (!m) return line;

				const [_, at, name, filename, lineNo] = m;
				const tracer = filenameToTracer(filename);
				if (!tracer) return line;

				const traced = originalPositionFor(tracer, {
					line: +lineNo,
					// QuickJS doesn't provide column number.
					// Unfortunately that means that minification
					// doesn't work well with source maps :(
					column: 0,
				});
				if (!traced.source || !traced.line) return line;

				const proto = filename === 'romfs:/runtime.js' ? 'nxjs' : 'app';
				return `${at}${traced.name || name} (${proto}:${
					traced.source
				}:${traced.line})`;
			} catch (_) {}
			return line;
		})
		.join('\n');
};
