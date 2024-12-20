import { $ } from './$';
import { bufferSourceToArrayBuffer, pathToString } from './utils';
import { File } from './polyfills/file';
import { decoder } from './polyfills/text-decoder';
import { encoder } from './polyfills/text-encoder';
import type { PathLike } from './switch';
import type { BufferSource } from './types';

export interface ReadFileOptions {
	/**
	 * Byte offset to start reading the file from.
	 *
	 * @default 0
	 */
	start?: number;

	/**
	 * Byte offset to stop reading the file at (inclusive).
	 *
	 * @default Infinity
	 */
	end?: number;
}

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
export function readFile(path: PathLike, opts?: ReadFileOptions) {
	return $.readFile(pathToString(path), opts);
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
export function readFileSync(path: PathLike, opts?: ReadFileOptions) {
	return $.readFileSync(pathToString(path), opts);
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
 * Removes the file or directory recursively specified by `path`.
 *
 * @param path File path to remove.
 */
export function remove(path: PathLike) {
	return $.remove(pathToString(path));
}

/**
 * Synchronously removes the file or directory recursively specified by `path`.
 *
 * @param path File path to remove.
 */
export function removeSync(path: PathLike) {
	$.removeSync(pathToString(path));
}

/**
 * Renames a file or directory.
 *
 * @param path Source file path to rename.
 * @param dest Destination file path to rename to.
 */
export function rename(path: PathLike, dest: PathLike) {
	return $.rename(pathToString(path), pathToString(dest));
}

/**
 * Synchronously renames a file or directory.
 *
 * @param path Source file path to rename.
 * @param dest Destination file path to rename to.
 */
export function renameSync(path: PathLike, dest: PathLike) {
	$.renameSync(pathToString(path), pathToString(dest));
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
	return $.stat(pathToString(path));
}

/**
 * Options object for the {@link file | `Switch.file()`} function.
 */
export interface FsFileOptions extends ReadFileOptions {
	type?: string;

	/**
	 * Create a "big file", which is a directory with the "archive" bit set.
	 * This will cause HOS to treat the directory as if it were a file
	 * containing the directory's concatenated contents, allowing you to
	 * write file contents larger than 4GB.
	 */
	bigFile?: boolean;
}

/**
 * Options object for the {@link FsFile.stream | `Switch.FsFile#stream()`} function.
 */
export interface FsFileStreamOptions {
	/**
	 * The size of each chunk to read from the file.
	 *
	 * @default 65536
	 */
	chunkSize?: number;
}

/**
 * Returns a {@link FsFile | `Switch.FsFile`} instance for the given `path`.
 *
 * @param path
 */
export function file(path: PathLike, opts?: FsFileOptions) {
	return new FsFile(path, opts);
}

/**
 * The `Switch.FsFile` class is a special implementation of the
 * global {@link File | `File`} class, which interacts with the
 * system's physical file system.
 *
 * It offers a convenient API for working with existing files, and
 * also for writing files.
 */
export class FsFile extends File {
	start?: number;
	end?: number;

	constructor(path: PathLike, opts?: FsFileOptions) {
		const { bigFile, start, end, ...rest } = opts ?? {};
		super([], pathToString(path), {
			type: 'text/plain;charset=utf-8',
			...rest,
		});
		this.start = start;
		this.end = end;
		Object.defineProperty(this, 'lastModified', {
			get(): number {
				return (statSync(this.name)?.mtime ?? 0) * 1000;
			},
		});
		if (bigFile) {
			$.fsCreateBigFile(this.name);
		}
	}

	get size() {
		const stat = statSync(this.name);
		if (!stat) return 0;
		const start = this.start ?? 0;
		const end = Math.min(this.end ?? Infinity, stat.size);
		return end - start;
	}

	stat() {
		return stat(this.name);
	}

	slice(start?: number, end?: number, type?: string): FsFile {
		const thisStart = this.start ?? 0;
		const thisEnd = this.end ?? Infinity;
		const s = (start ?? 0) + thisStart;
		const newEnd = thisStart + (end ?? Infinity);
		const e = Math.min(thisEnd, newEnd);
		return new FsFile(this.name, {
			type: type ?? this.type,
			start: s,
			end: e,
		});
	}

	async arrayBuffer(): Promise<ArrayBuffer> {
		const b = await readFile(this.name, this);
		if (!b) {
			throw new Error(`File does not exist: "${this.name}"`);
		}
		return b;
	}

	async text(): Promise<string> {
		return decoder.decode(await this.arrayBuffer());
	}

	async json(): Promise<any> {
		return JSON.parse(await this.text());
	}

	stream(opts?: FsFileStreamOptions): ReadableStream<Uint8Array> {
		const { name } = this;
		const chunkSize = opts?.chunkSize || 65536;
		let h: Awaited<ReturnType<typeof $.fopen>> | undefined | null;
		return new ReadableStream({
			type: 'bytes',
			async pull(controller) {
				if (!h) h = await $.fopen(name, 'rb');
				const b = new Uint8Array(chunkSize);
				const n = await $.fread(h, b.buffer);
				if (n === null) {
					controller.close();
					await $.fclose(h);
					h = null;
				} else if (n > 0) {
					controller.enqueue(n < b.length ? b.subarray(0, n) : b);
				}
			},
			async cancel() {
				if (h) {
					await $.fclose(h);
				}
			},
		});
	}

	get writable() {
		const { name } = this;
		let h: Awaited<ReturnType<typeof $.fopen>> | undefined | null;
		return new WritableStream<BufferSource | string>({
			async write(chunk) {
				if (!h) h = await $.fopen(name, 'w');
				await $.fwrite(
					h,
					typeof chunk === 'string'
						? encoder.encode(chunk).buffer
						: bufferSourceToArrayBuffer(chunk),
				);
			},
			async close() {
				if (h) {
					await $.fclose(h);
					h = null;
				}
			},
		});
	}
}
