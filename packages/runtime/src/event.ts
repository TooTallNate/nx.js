export class Event implements globalThis.Event {
	static readonly NONE = 0 as const;
	static readonly CAPTURING_PHASE = 1 as const;
	static readonly AT_TARGET = 2 as const;
	static readonly BUBBLING_PHASE = 3 as const;

	readonly NONE = 0 as const;
	readonly CAPTURING_PHASE = 1 as const;
	readonly AT_TARGET = 2 as const;
	readonly BUBBLING_PHASE = 3 as const;

	readonly bubbles: boolean;
	cancelBubble: boolean;
	readonly cancelable: boolean;
	readonly composed: boolean;
	readonly currentTarget: EventTarget | null;
	readonly defaultPrevented: boolean;
	readonly eventPhase: number;
	readonly isTrusted: boolean;
	returnValue: boolean;
	readonly srcElement: EventTarget | null;
	readonly target: EventTarget | null;
	readonly timeStamp: number;
	readonly type: string;
	constructor(type: string, options?: EventInit) {
		this.type = type;
		this.bubbles =
			this.cancelable =
			this.cancelBubble =
			this.composed =
			this.defaultPrevented =
			this.isTrusted =
			this.returnValue =
				false;
		this.currentTarget = this.srcElement = this.target = null;
		this.eventPhase = this.timeStamp = 0;
		this.cancelable = false;
		this.cancelBubble = false;
		this.composed = false;
		if (options) {
			this.bubbles = options.bubbles ?? false;
			this.cancelable = options.cancelable ?? false;
		}
	}
	composedPath(): EventTarget[] {
		throw new Error('Method not implemented.');
	}
	initEvent(
		type: string,
		bubbles?: boolean | undefined,
		cancelable?: boolean | undefined
	): void {
		throw new Error('Method not implemented.');
	}
	preventDefault(): void {
		// @ts-expect-error - `defaultPrevented` is readonly
		this.defaultPrevented = true;
	}
	stopImmediatePropagation(): void {
		throw new Error('Method not implemented.');
	}
	stopPropagation(): void {
		throw new Error('Method not implemented.');
	}
}

export class UIEvent extends Event implements globalThis.UIEvent {
	readonly detail: number;
	readonly view!: Window | null;
	readonly which!: number;
	constructor(type: string, options?: UIEventInit) {
		super(type, options);
		this.detail = 0;
		if (options) {
			this.detail = options.detail ?? 0;
		}
	}
	initUIEvent(
		type: string,
		bubbles?: boolean | undefined,
		cancelable?: boolean | undefined,
		view?: Window | null | undefined,
		detail?: number | undefined
	): void {
		throw new Error('Method not implemented.');
	}
}

export class TouchEvent extends UIEvent implements globalThis.TouchEvent {
	readonly altKey: boolean;
	readonly changedTouches: TouchList;
	readonly ctrlKey: boolean;
	readonly metaKey: boolean;
	readonly shiftKey: boolean;
	readonly targetTouches: TouchList;
	readonly touches: TouchList;

	constructor(type: string, options: TouchEventInit) {
		super(type, options);
		this.altKey = this.ctrlKey = this.metaKey = this.shiftKey = false;
		// @ts-expect-error
		this.changedTouches = options.changedTouches ?? [];
		// @ts-expect-error
		this.targetTouches = options.targetTouches ?? [];
		// @ts-expect-error
		this.touches = options.touches ?? [];
	}
}
