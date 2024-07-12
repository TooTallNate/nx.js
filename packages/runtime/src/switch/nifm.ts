import { $ } from '../$';
import { runOnce } from '../utils';

const init = runOnce(() => addEventListener('unload', $.nifmInitialize()));

export function networkInfo() {
	init();
	return $.networkInfo();
}
