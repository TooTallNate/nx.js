export const def = (key: string, value: unknown) =>
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: false,
		configurable: true,
	});

import 'core-js/actual/url';
import 'core-js/actual/url-search-params';
import EventTarget from '@ungap/event-target';
def('EventTarget', EventTarget);
