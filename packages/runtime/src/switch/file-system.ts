import { $ } from '../$';
import { crypto } from '../crypto';
import { URL } from '../polyfills/url';
import { assertInternalConstructor, proto, stub } from '../utils';

const genName = () => `f${crypto.randomUUID().replace(/-/g, '').slice(0, 16)}`;

export class FileSystem {
	/**
	 * A `URL` instance that points to the root of the filesystem mount.
	 * You should use this to create file path references within
	 * the filesystem mount.
	 *
	 * @example
	 *
	 * ```typescript
	 * const dataUrl = new URL('data.json', fileSystem.url);
	 * ```
	 */
	declare url: URL | null;

	/**
	 * @private
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	freeSpace(): bigint {
		stub();
	}

	totalSpace(): bigint {
		stub();
	}

	/**
	 * Mounts the `FileSystem` such that filesystem operations may be used.
	 *
	 * @param name The name of the mount for filesystem paths. By default, a random name is generated. Shouldn't exceed 31 characters, and shouldn't have a trailing colon.
	 */
	mount(name = genName()) {
		$.fsMount(this, name);
		this.url = new URL(`${name}:/`);
		return this.url;
	}

	/**
	 * Opens a file system partition specified by its `BisPartitionId`.
	 *
	 * @example
	 *
	 * ```typescript
	 * import { BisPartitionId } from '@nx.js/constants';
	 *
	 * // Open and mount the "User" partition
	 * const fs = Switch.FileSystem.openBis(BisPartitionId.User);
	 * const url = fs.mount();
	 *
	 * // Read the file entries at the root of the partition
	 * console.log(Switch.readDirSync(url));
	 * ```
	 *
	 * @param id The `BisPartitionId` of the partition to open.
	 */
	static openBis(id: number): FileSystem {
		return proto($.fsOpenBis(id), FileSystem);
	}
}
$.fsInit(FileSystem);
