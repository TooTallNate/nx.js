import { $ } from '../$';
import { URL } from '../polyfills/url';

/**
 * Represents a mounted filesystem device, such as the Save Data store for an Application / Profile pair.
 */
export class FsDev {
	/**
	 * A `URL` instance that points to the root of the filesystem mount. You should use this to create file path references within the filesystem mount.
	 *
	 * @example
	 *
	 * ```typescript
	 * const dataUrl = new URL('data.json', device.url);
	 * ```
	 */
	url: URL;

	/**
	 * @ignore
	 */
	constructor(name: string) {
		this.url = new URL(`${name}:/`);
	}

	/**
	 * Commits to the disk any write operations that have occurred on this filesystem mount since the previous commit.
	 *
	 * Failure to call this function after writes will cause the data to be lost after the application exits.
	 *
	 * @example
	 *
	 * ```typescript
	 * const saveStateUrl = new URL('state', device.url);
	 * Switch.writeFileSync(saveStateUrl, 'my application state...');
	 *
	 * device.commit(); // Write operation is persisted to the disk
	 * ```
	 */
	commit() {
		$.fsdevCommitDevice(this.url.protocol);
	}

	/**
	 * Unmounts the filesystem mount. Any filesytem operations attempting to use the mount path in the future will throw an error.
	 *
	 * @example
	 *
	 * ```typescript
	 * Switch.readFileSync(device.url); // OK
	 *
	 * device.unmount();
	 *
	 * Switch.readFileSync(device.url); // ERROR THROWN!
	 * ```
	 */
	unmount() {
		$.fsdevUnmountDevice(this.url.protocol);
	}
}
