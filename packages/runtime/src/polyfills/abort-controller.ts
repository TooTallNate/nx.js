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
			signalReason = new Error('This operation was aborted');
			signalReason.name = 'AbortError';
		}
		Object.defineProperty(this.signal, 'reason', { value: signalReason });

		this.signal.dispatchEvent(event);
	}
}

def(AbortSignal);
def(AbortController, 'AbortController');
