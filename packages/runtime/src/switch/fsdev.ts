import { $ } from '../$';

/**
 * Represents a mounted filesystem device, such as the Save Data store for an Application / Profile pair.
 */
export class FsDev {
	name: string;

	constructor(name: string) {
		this.name = name;
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
		$.fsdevCommitDevice(this.name);
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
		$.fsdevUnmountDevice(this.name);
	}
}
