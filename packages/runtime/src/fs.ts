import { $ } from './$';
import { bufferSourceToArrayBuffer, pathToString, toPromise } from './utils';
import { encoder } from './polyfills/text-encoder';
import type { PathLike } from './switch';

/**
 * Creates the directory at the provided `path`, as well as any necessary parent directories.
 *
 * @example
 *
 * ```typescript
 * const count = Switch.mkdirSync('sdmc:/foo/bar/baz');
 * console.log(`Created ${count} directories`);
 * // Created 3 directories
 * ```
 *
 * @param path Path of the directory to create.
 * @param mode The file mode to set for the directories. Default: `0o777`.
 * @returns The number of directories created. If the directory already exists, returns `0`.
 */
export function mkdirSync(path: PathLike, mode = 0o777) {
	return $.mkdirSync(pathToString(path), mode);
}

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
 * @example
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
 * Synchronously removes the file or directory recursively specified by `path`.
 *
 * @param path File path to remove.
 */
export function removeSync(path: PathLike) {
	return $.removeSync(pathToString(path));
}

/**
 * Removes the file or directory recursively specified by `path`.
 *
 * @param path File path to remove.
 */
export function remove(path: PathLike) {
	return toPromise($.remove, pathToString(path));
}

/**
 *
 * @param path File path to retrieve file stats for.
 * @returns Object containing the file stat information of `path`, or `null` if the file does not exist.
 */
export function statSync(path: PathLike) {
	return $.statSync(pathToString(path));
}

/**
 * Returns a Promise which resolves to an object containing
 * information about the file pointed to by `path`.
 *
 * @param path File path to retrieve file stats for.
 */
export function stat(path: PathLike) {
	return toPromise($.stat, pathToString(path));
}
