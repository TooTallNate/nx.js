import { $ } from '../$';
import { File } from '../polyfills/file';
import { proto, stub } from '../utils';

let init = false;

function _init() {
	if (init) return;
	addEventListener('unload', $.capsaInitialize());
	init = true;
}

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

export class AlbumFile extends File {
	constructor(storage: number, id: string) {
		_init();
		super([], '');
		//return proto($.albumFileNew(storage, id), AlbumFile);
	}

	/**
	 * Loads the thumbnail JPEG image for the album file.
	 */
	thumbnail(): Promise<ArrayBuffer> {
		stub();
	}
}
$.albumFileInit(AlbumFile);
