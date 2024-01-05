import { INTERNAL_SYMBOL } from './internal';
import { assertInternalConstructor, def } from './utils';
import type { DOMHighResTimeStamp } from './types';

/**
 * @see https://developer.mozilla.org/en-US/docs/Web/API/Performance_API
 */
export class Performance {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/timeOrigin
	 */
	readonly timeOrigin: DOMHighResTimeStamp;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		this.timeOrigin = Date.now();
	}

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/Performance/now
	 */
	now(): DOMHighResTimeStamp {
		return Date.now() - this.timeOrigin;
	}
}
def('Performance', Performance);

// @ts-expect-error Internal constructor
export var performance = new Performance(INTERNAL_SYMBOL);
def('performance', performance);
