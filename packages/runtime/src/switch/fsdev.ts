import { $ } from '../$';

/**
 * Represents a mounted filesystem device, such as the Save Data store for an Application / Profile pair.
 */
export class FsDev {
	name: string;

	constructor(name: string) {
		this.name = name;
	}

	commit() {
		$.fsdevCommitDevice(this.name);
	}

	unmount() {
		$.fsdevUnmountDevice(this.name);
	}
}
