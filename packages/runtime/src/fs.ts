import { $ } from './$';
import { bufferSourceToArrayBuffer, pathToString, toPromise } from './utils';
import { encoder } from './polyfills/text-encoder';
import type { PathLike } from './types';

/**
 * Returns a Promise which resolves to an `ArrayBuffer` containing
 * the contents of the file at `path`.
 *
 * @example
 *
 * ```typescript
 * const buffer = await Switch.readFile('sdmc:/switch/awesome-app/state.json');
 * const gameState = JSON.parse(new TextDecoder().decode(buffer));
 * ```
 */
export function readFile(path: PathLike) {
	return toPromise($.readFile, pathToString(path));
}

/**
 * Synchronously returns an array of the file names within `path`.
 *
 * @example
 *
 * ```typescript
 * for (const file of Switch.readDirSync('sdmc:/')) {
 *   // … do something with `file` …
 * }
 * ```
 */
export function readDirSync(path: PathLike) {
	return $.readDirSync(pathToString(path));
}

/**
 * Synchronously returns an `ArrayBuffer` containing the contents
 * of the file at `path`.
 *
 * @example
 *
 * ```typescript
 * const buffer = Switch.readFileSync('sdmc:/switch/awesome-app/state.json');
 * const appState = JSON.parse(new TextDecoder().decode(buffer));
 * ```
 */
export function readFileSync(path: PathLike) {
	return $.readFileSync(pathToString(path));
}

/**
 * Synchronously writes the contents of `data` to the file at `path`.
 *
 * ```typescript
 * const appStateJson = JSON.stringify(appState);
 * Switch.writeFileSync('sdmc:/switch/awesome-app/state.json', appStateJson);
 * ```
 */
export function writeFileSync(path: PathLike, data: string | BufferSource) {
	const d = typeof data === 'string' ? encoder.encode(data) : data;
	const ab = bufferSourceToArrayBuffer(d);
	return $.writeFileSync(pathToString(path), ab);
}

/**
 * Removes the file or directory specified by `path`.
 */
export function remove(path: PathLike) {
	return toPromise($.remove, pathToString(path));
}

/**
 * Returns a Promise which resolves to an object containing
 * information about the file pointed to by `path`.
 */
export function stat(path: PathLike) {
	return toPromise($.stat, pathToString(path));
}
