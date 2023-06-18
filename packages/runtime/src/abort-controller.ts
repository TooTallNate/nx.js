// Based on https://github.com/mo/abortcontroller-polyfill
// By @mo - MIT License

export class AbortSignal extends EventTarget implements globalThis.AbortSignal {
	readonly reason!: any;
	readonly aborted!: boolean;
	onabort!: ((this: globalThis.AbortSignal, ev: Event) => any) | null;

	constructor() {
		super();

		// Compared to assignment, Object.defineProperty makes properties non-enumerable by default and
		// we want Object.keys(new AbortController().signal) to be [] for compat with the native impl
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
		throw new Error('Method not implemented.');
	}
	dispatchEvent(event: Event): boolean {
		if (event.type === 'abort') {
			Object.defineProperty(this, 'aborted', { value: true });
			if (typeof this.onabort === 'function') {
				this.onabort.call(this, event);
			}
		}
		return super.dispatchEvent(event);
	}
}
Object.defineProperty(AbortSignal.prototype, Symbol.toStringTag, {
	value: 'AbortSignal',
});

export class AbortController implements globalThis.AbortController {
	signal!: globalThis.AbortSignal;

	constructor() {
		// Compared to assignment, Object.defineProperty makes properties non-enumerable by default and
		// we want Object.keys(new AbortController()) to be [] for compat with the native impl
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
Object.defineProperty(AbortController.prototype, Symbol.toStringTag, {
	value: 'AbortController',
});
