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

	/**
	 * The amount of free space available on the filesystem, in bytes.
	 */
	freeSpace(): bigint {
		stub();
	}

	/**
	 * The total amount of space available on the filesystem, in bytes.
	 */
	totalSpace(): bigint {
		stub();
	}

	/**
	 * Mounts the `FileSystem` such that filesystem operations may be used.
	 *
	 * @param name The name of the mount for filesystem paths. By default, a random name is generated. Should not exceed 31 characters, and should not have a trailing colon.
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

	/**
	 * Opens a file system partition for the SD card.
	 *
	 * Note that the SD card is automatically mounted under the `sdmc:` protocol,
	 * so your application will not need to call this function under most circumstances.
	 * However, it is useful for querying metatdata about the SD card, such as
	 * the amount of free space available.
	 *
	 * @example
	 *
	 * ```typescript
	 * const fs = Switch.FileSystem.openSdmc();
	 * console.log(fs.freeSpace());
	 * console.log(fs.totalSpace());
	 * ```
	 */
	static openSdmc(): FileSystem {
		return proto($.fsOpenSdmc(), FileSystem);
	}
}
$.fsInit(FileSystem);
