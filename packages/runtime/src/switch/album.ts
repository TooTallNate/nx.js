import { $ } from '../$';
import { inspect } from '../inspect';
import { File } from '../polyfills/file';
import { proto, stub } from '../utils';
import type { Blob } from '../polyfills/blob';

let init = false;

function _init() {
	if (init) return;
	addEventListener('unload', $.capsaInitialize());
	init = true;
}

/**
 * The `Switch.Album` class allows for interacting with the Switch's photo gallery,
 * providing access to the screenshots / video recordings that the user has saved.
 *
 * It is a `Set` subclass, which contains entries of
 * {@link AlbumFile | `Switch.AlbumFile`} instances.
 *
 * @example
 *
 * ```typescript
 * import { CapsAlbumStorage } from '@nx.js/constants';
 *
 * const album = new Switch.Album(CapsAlbumStorage.Sd);
 * for (const file of album) {
 *   console.log(file);
 * }
 * ```
 */
export class Album extends Set<AlbumFile> {
	readonly storage: number;

	constructor(storage: number) {
		_init();
		super();
		this.storage = storage;
	}

	*values(): IterableIterator<AlbumFile> {
		for (const f of $.albumFileList(this)) {
			yield proto(f, AlbumFile);
		}
	}

	keys(): IterableIterator<AlbumFile> {
		return this.values();
	}

	*entries(): IterableIterator<[AlbumFile, AlbumFile]> {
		for (const f of this) {
			yield [f, f];
		}
	}

	[Symbol.iterator]() {
		return this.values();
	}
}
$.albumInit(Album);

/**
 * Represents a file within a {@link Album | `Switch.Album`} content store,
 * which is either a screenshot (JPEG image) or a screen recording (MP4 movie).
 *
 * It is a subclass of `File`, so you can use familiar features like `name`,
 * `lastModified` and `arrayBuffer()`. It also has additional metadata like
 * `applicationId` to determine which application generated the contents.
 */
export class AlbumFile extends File {
	/**
	 * The ID of the application which generated the album file.
	 */
	declare applicationId: bigint;

	/**
	 * The type of content which the album file contains. The value
	 * corresponds with the `CapsAlbumFileContents` enum from `@nx.js/constants`.
	 */
	declare content: number;

	/**
	 * The storage device which contains the album file. The value
	 * corresponds with the `CapsAlbumStorage` enum from `@nx.js/constants`.
	 */
	declare storage: number;

	/**
	 * Unique ID for when there's multiple album files with the same timestamp.
	 *
	 * @note The value is usually `0`.
	 */
	declare id: number;

	constructor(storage: number, id: string) {
		_init();
		super([], '');
		throw new Error('Method not implemented.');
		//return proto($.albumFileNew(storage, id), AlbumFile);
	}

	text(): Promise<string> {
		throw new Error('Not supported');
	}

	slice(start?: number, end?: number, type?: string): Blob {
		throw new Error('Not supported');
	}

	stream(): ReadableStream<Uint8Array> {
		throw new Error('Method not implemented.');
	}

	/**
	 * Loads the thumbnail JPEG image for the album file.
	 *
	 * @example
	 *
	 * ```typescript
	 * const ctx = screen.getContext('2d');
	 * const buf = await file.arrayBuffer();
	 * const img = new Image();
	 * img.onload = () => {
	 *   ctx.drawImage(img, 0, 0);
	 * };
	 * img.src = URL.createObjectURL(new Blob([buf]));
	 * ```
	 */
	thumbnail(): Promise<ArrayBuffer> {
		stub();
	}
}
$.albumFileInit(AlbumFile);

Object.defineProperty(AlbumFile.prototype, inspect.keys, {
	enumerable: false,
	value: () => ['applicationId', 'storage', 'content', 'id', 'size', 'type'],
});
