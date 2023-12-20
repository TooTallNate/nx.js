import { $ } from '../$';

let nifmInitialized = false;

export function networkInfo() {
	if (!nifmInitialized) {
		addEventListener('unload', $.nifmInitialize());
		nifmInitialized = true;
	}
	return $.networkInfo();
}
