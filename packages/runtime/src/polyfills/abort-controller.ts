// Based on https://github.com/mo/abortcontroller-polyfill
// By @mo - MIT License

import { def } from '../utils';
import { Event } from './event';
import { EventTarget } from './event-target';

export class AbortSignal extends EventTarget implements globalThis.AbortSignal {
	readonly reason!: any;
	readonly aborted!: boolean;
	onabort!: ((this: AbortSignal, ev: Event) => any) | null;

	constructor() {
		super();
		Object.defineProperty(this, 'aborted', {
			value: false,
			writable: true,
			configurable: true,
		});
		Object.defineProperty(this, 'onabort', {
			value: null,
			writable: true,
			configurable: true,
		});
		Object.defineProperty(this, 'reason', {
			value: undefined,
			writable: true,
			configurable: true,
		});
	}

	throwIfAborted(): void {
		if (this.aborted) {
			throw this.reason;
		}
	}

	dispatchEvent(event: Event): boolean {
		if (event.type === 'abort') {
			Object.defineProperty(this, 'aborted', { value: true });
			if (typeof this.onabort === 'function') {
				this.onabort(event);
			}
		}
		return super.dispatchEvent(event);
	}

	/**
	 * Returns an `AbortSignal` that is already set as aborted.
	 * @see https://developer.mozilla.org/docs/Web/API/AbortSignal/abort_static
	 */
	static abort(reason?: any): AbortSignal {
		const signal = new AbortSignal();
		Object.defineProperty(signal, 'aborted', { value: true });
		Object.defineProperty(signal, 'reason', {
			value:
				reason !== undefined
					? reason
					: new DOMException(
							'The operation was aborted.',
							'AbortError',
						),
		});
		return signal;
	}

	/**
	 * Returns an `AbortSignal` that automatically aborts after the given number of milliseconds.
	 * @see https://developer.mozilla.org/docs/Web/API/AbortSignal/timeout_static
	 */
	static timeout(ms: number): AbortSignal {
		const controller = new AbortController();
		setTimeout(() => {
			controller.abort(
				new DOMException('The operation timed out.', 'TimeoutError'),
			);
		}, ms);
		return controller.signal;
	}

	// Instance method required by TypeScript's AbortSignal interface (TS incorrectly
	// defines `any` as instance rather than static). Keep both to satisfy the type checker.
	any(signals: Iterable<AbortSignal>): AbortSignal {
		return AbortSignal.any(signals);
	}

	/**
	 * Returns an `AbortSignal` that aborts when any of the given signals abort.
	 * @see https://developer.mozilla.org/docs/Web/API/AbortSignal/any_static
	 */
	static any(signals: Iterable<AbortSignal>): AbortSignal {
		const controller = new AbortController();
		for (const signal of signals) {
			if (signal.aborted) {
				controller.abort(signal.reason);
				return controller.signal;
			}
			signal.addEventListener('abort', () => {
				if (!controller.signal.aborted) {
					controller.abort(signal.reason);
				}
			});
		}
		return controller.signal;
	}
}

export class AbortController implements globalThis.AbortController {
	signal!: AbortSignal;

	constructor() {
		Object.defineProperty(this, 'signal', {
			value: new AbortSignal(),
			writable: true,
			configurable: true,
		});
	}
	abort(reason?: any) {
		const event = new Event('abort');

		let signalReason = reason;
		if (signalReason === undefined) {
			signalReason = new DOMException(
				'The operation was aborted.',
				'AbortError',
			);
		}
		Object.defineProperty(this.signal, 'reason', { value: signalReason });

		this.signal.dispatchEvent(event);
	}
}

def(AbortSignal);
def(AbortController, 'AbortController');
