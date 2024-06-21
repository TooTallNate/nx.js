import { assertInternalConstructor, createInternal, def } from '../utils';
import type { EventTarget } from './event-target';
import type { Gamepad } from '../navigator/gamepad';

export interface EventInit {
	bubbles?: boolean;
	cancelable?: boolean;
	composed?: boolean;
}

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
		cancelable?: boolean | undefined,
	): void {
		throw new Error('Method not implemented.');
	}
	preventDefault(): void {
		if (this.cancelable) {
			// @ts-expect-error - `defaultPrevented` is readonly
			this.defaultPrevented = true;
		}
	}
	stopImmediatePropagation(): void {
		throw new Error('Method not implemented.');
	}
	stopPropagation(): void {
		throw new Error('Method not implemented.');
	}
}

export interface CustomEventInit<T = any> extends EventInit {
	detail?: T;
}

export class CustomEvent<T = any>
	extends Event
	implements globalThis.CustomEvent
{
	readonly detail: T | null;

	constructor(type: string, options: CustomEventInit<T> = {}) {
		super(type, options);
		this.detail = options.detail ?? null;
	}

	initCustomEvent(): void {
		throw new Error('Method not implemented.');
	}
}

export interface UIEventInit extends EventInit {
	detail?: number;
}

export class UIEvent extends Event implements globalThis.UIEvent {
	readonly detail: number;
	readonly view: null;
	readonly which: number;
	constructor(type: string, options?: UIEventInit) {
		super(type, options);
		this.view = null;
		this.which = -1;
		this.detail = options?.detail ?? 0;
	}
	initUIEvent(): void {
		throw new Error('Method not implemented.');
	}
}

// Keyboard modifiers bitmasks
const CTRL = 1n << 0n;
const SHIFT = 1n << 1n;
const ALT = (1n << 2n) | (1n << 3n);
const META = 1n << 4n;

/// HidKeyboardKey
enum KeyboardKey {
	A = 4,
	B = 5,
	C = 6,
	D = 7,
	E = 8,
	F = 9,
	G = 10,
	H = 11,
	I = 12,
	J = 13,
	K = 14,
	L = 15,
	M = 16,
	N = 17,
	O = 18,
	P = 19,
	Q = 20,
	R = 21,
	S = 22,
	T = 23,
	U = 24,
	V = 25,
	W = 26,
	X = 27,
	Y = 28,
	Z = 29,
	Digit1 = 30,
	Digit2 = 31,
	Digit3 = 32,
	Digit4 = 33,
	Digit5 = 34,
	Digit6 = 35,
	Digit7 = 36,
	Digit8 = 37,
	Digit9 = 38,
	Digit0 = 39,
	Enter = 40,
	Escape = 41,
	Backspace = 42,
	Tab = 43,
	Space = 44,
	Minus = 45,
	Equal = 46,
	BracketLeft = 47,
	BracketRight = 48,
	Backslash = 49,
	Tilde = 50,
	Semicolon = 51,
	Quote = 52,
	Backquote = 53,
	Comma = 54,
	Period = 55,
	Slash = 56,
	CapsLock = 57,
	F1 = 58,
	F2 = 59,
	F3 = 60,
	F4 = 61,
	F5 = 62,
	F6 = 63,
	F7 = 64,
	F8 = 65,
	F9 = 66,
	F10 = 67,
	F11 = 68,
	F12 = 69,
	PrintScreen = 70,
	ScrollLock = 71,
	Pause = 72,
	Insert = 73,
	Home = 74,
	PageUp = 75,
	Delete = 76,
	End = 77,
	PageDown = 78,
	ArrowRight = 79,
	ArrowLeft = 80,
	ArrowDown = 81,
	ArrowUp = 82,
	NumLock = 83,
	NumpadDivide = 84,
	NumpadMultiply = 85,
	NumpadSubtract = 86,
	NumpadAdd = 87,
	NumpadEnter = 88,
	Numpad1 = 89,
	Numpad2 = 90,
	Numpad3 = 91,
	Numpad4 = 92,
	Numpad5 = 93,
	Numpad6 = 94,
	Numpad7 = 95,
	Numpad8 = 96,
	Numpad9 = 97,
	Numpad0 = 98,
	NumpadDecimal = 99,
	//Backslash = 100,
	Application = 101,
	Power = 102,
	NumPadEquals = 103,
	F13 = 104,
	F14 = 105,
	F15 = 106,
	F16 = 107,
	F17 = 108,
	F18 = 109,
	F19 = 110,
	F20 = 111,
	F21 = 112,
	F22 = 113,
	F23 = 114,
	F24 = 115,
	NumPadComma = 133,
	Ro = 135,
	KatakanaHiragana = 136,
	Yen = 137,
	Henkan = 138,
	Muhenkan = 139,
	NumPadCommaPc98 = 140,
	HangulEnglish = 144,
	Hanja = 145,
	Katakana = 146,
	Hiragana = 147,
	ZenkakuHankaku = 148,
	ControlLeft = 224,
	ShiftLeft = 225,
	AltLeft = 226,
	OSLeft = 227,
	ControlRight = 228,
	ShiftRight = 229,
	AltRight = 230,
	OSRight = 231,
}

const keyboardKeyMap = new Map<KeyboardKey, string | [string, string]>([
	[KeyboardKey.Digit1, ['1', '!']],
	[KeyboardKey.Digit2, ['2', '@']],
	[KeyboardKey.Digit3, ['3', '#']],
	[KeyboardKey.Digit4, ['4', '$']],
	[KeyboardKey.Digit5, ['5', '%']],
	[KeyboardKey.Digit6, ['6', '^']],
	[KeyboardKey.Digit7, ['7', '&']],
	[KeyboardKey.Digit8, ['8', '*']],
	[KeyboardKey.Digit9, ['9', '(']],
	[KeyboardKey.Digit0, ['0', ')']],
	[KeyboardKey.Space, ' '],
	[KeyboardKey.Minus, ['-', '_']],
	[KeyboardKey.Equal, ['=', '+']],
	[KeyboardKey.BracketLeft, ['[', '{']],
	[KeyboardKey.BracketRight, [']', '}']],
	[KeyboardKey.Backslash, ['\\', '|']],
	[KeyboardKey.Tilde, '~'],
	[KeyboardKey.Semicolon, [';', ':']],
	[KeyboardKey.Quote, ["'", '"']],
	[KeyboardKey.Backquote, ['`', '~']],
	[KeyboardKey.Comma, [',', '<']],
	[KeyboardKey.Period, ['.', '>']],
	[KeyboardKey.Slash, ['/', '?']],
	[KeyboardKey.NumpadDivide, '/'],
	[KeyboardKey.NumpadMultiply, '*'],
	[KeyboardKey.NumpadSubtract, '-'],
	[KeyboardKey.NumpadAdd, '+'],
	[KeyboardKey.NumpadEnter, 'Enter'],
	[KeyboardKey.Numpad1, '1'],
	[KeyboardKey.Numpad2, '2'],
	[KeyboardKey.Numpad3, '3'],
	[KeyboardKey.Numpad4, '4'],
	[KeyboardKey.Numpad5, '5'],
	[KeyboardKey.Numpad6, '6'],
	[KeyboardKey.Numpad7, '7'],
	[KeyboardKey.Numpad8, '8'],
	[KeyboardKey.Numpad9, '9'],
	[KeyboardKey.Numpad0, '0'],
	[KeyboardKey.NumpadDecimal, '.'],
	[KeyboardKey.ControlLeft, 'Control'],
	[KeyboardKey.ShiftLeft, 'Shift'],
	[KeyboardKey.AltLeft, 'Alt'],
	[KeyboardKey.OSLeft, 'Meta'],
	[KeyboardKey.ControlRight, 'Control'],
	[KeyboardKey.ShiftRight, 'Shift'],
	[KeyboardKey.AltRight, 'Alt'],
	[KeyboardKey.OSRight, 'Meta'],
]);

export interface EventModifierInit extends UIEventInit {
	altKey?: boolean;
	ctrlKey?: boolean;
	metaKey?: boolean;
	modifierAltGraph?: boolean;
	modifierCapsLock?: boolean;
	modifierFn?: boolean;
	modifierFnLock?: boolean;
	modifierHyper?: boolean;
	modifierNumLock?: boolean;
	modifierScrollLock?: boolean;
	modifierSuper?: boolean;
	modifierSymbol?: boolean;
	modifierSymbolLock?: boolean;
	shiftKey?: boolean;
}

export interface KeyboardEventInit extends EventModifierInit {
	/** @deprecated */
	charCode?: number;
	code?: string;
	isComposing?: boolean;
	key?: string;
	/** @deprecated */
	keyCode?: number;
	location?: number;
	repeat?: boolean;
}

const _ = createInternal<KeyboardEvent, bigint>();

export class KeyboardEvent extends UIEvent implements globalThis.KeyboardEvent {
	readonly DOM_KEY_LOCATION_STANDARD = 0 as const;
	readonly DOM_KEY_LOCATION_LEFT = 1 as const;
	readonly DOM_KEY_LOCATION_RIGHT = 2 as const;
	readonly DOM_KEY_LOCATION_NUMPAD = 3 as const;
	readonly charCode: number;
	readonly isComposing: boolean;
	readonly keyCode: number;
	readonly location: number;
	readonly repeat: boolean;

	constructor(type: string, options?: KeyboardEventInit) {
		super(type, options);
		let modifiers = 0n;
		if (options) {
			this.charCode = options.charCode ?? -1;
			this.isComposing = options.isComposing ?? false;
			this.keyCode = options.keyCode ?? -1;
			this.location = options.location ?? -1;
			this.repeat = options.repeat ?? false;
			// @ts-expect-error
			modifiers = options.modifiers;
		} else {
			this.charCode = this.keyCode = this.location = -1;
			this.isComposing = this.repeat = false;
		}
		_.set(this, modifiers);
	}

	getModifierState(): boolean {
		throw new Error('Method not implemented.');
	}

	initKeyboardEvent(): void {
		throw new Error('Method not implemented.');
	}

	get ctrlKey(): boolean {
		return (_(this) & CTRL) !== 0n;
	}

	get shiftKey(): boolean {
		return (_(this) & SHIFT) !== 0n;
	}

	get altKey(): boolean {
		return (_(this) & ALT) !== 0n;
	}

	get metaKey(): boolean {
		return (_(this) & META) !== 0n;
	}

	get code(): string {
		return KeyboardKey[this.keyCode];
	}

	get key(): string {
		const { code } = this;
		let key = keyboardKeyMap.get(this.keyCode);
		if (typeof key === 'string') {
			return key;
		}
		if (Array.isArray(key)) {
			return this.shiftKey ? key[1] : key[0];
		}
		if (typeof code !== 'string') {
			// Sometimes get `keyCode=1`, which is not a known key
			return '';
		}
		if (code.length === 1) {
			// One of the alphabetic keys
			return this.shiftKey ? code : code.toLowerCase();
		}
		return code;
	}
}

export interface TouchInit {
	clientX?: number;
	clientY?: number;
	force?: number;
	identifier: number;
	pageX?: number;
	pageY?: number;
	radiusX?: number;
	radiusY?: number;
	rotationAngle?: number;
	screenX?: number;
	screenY?: number;
	target: EventTarget;
}

/**
 * A single contact point on a touch-sensitive device. The contact point is commonly a finger or stylus and the device may be a touchscreen or trackpad.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch)
 */
export class Touch implements globalThis.Touch {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/clientX) */
	readonly clientX: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/clientY) */
	readonly clientY: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/force) */
	readonly force: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/identifier) */
	readonly identifier: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/pageX) */
	readonly pageX: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/pageY) */
	readonly pageY: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/radiusX) */
	readonly radiusX: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/radiusY) */
	readonly radiusY: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/rotationAngle) */
	readonly rotationAngle: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/screenX) */
	readonly screenX: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/screenY) */
	readonly screenY: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Touch/target) */
	readonly target: EventTarget;

	constructor(init: TouchInit) {
		this.clientX = init.clientX ?? 0;
		this.clientY = init.clientY ?? 0;
		this.force = init.force ?? 0;
		this.identifier = init.identifier;
		this.pageX = init.pageX ?? 0;
		this.pageY = init.pageY ?? 0;
		this.radiusX = init.radiusX ?? 0;
		this.radiusY = init.radiusY ?? 0;
		this.rotationAngle = init.rotationAngle ?? 0;
		this.screenX = init.screenX ?? 0;
		this.screenY = init.screenY ?? 0;
		this.target = init.target;
	}
}
def(Touch);

/**
 * A list of contact points on a touch surface. For example, if the user has three
 * fingers on the touch surface (such as a screen or trackpad), the corresponding
 * `TouchList` object would have one `Touch` object for each finger, for a total
 * of three entries.
 *
 * [MDN Reference](https://developer.mozilla.org/docs/Web/API/TouchList)
 */
export class TouchList implements globalThis.TouchList {
	[index: number]: Touch;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TouchList/length) */
	declare readonly length: number;

	constructor() {
		assertInternalConstructor(arguments);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/TouchList/item) */
	item(index: number): Touch | null {
		if (typeof index !== 'number' || index >= this.length) return null;
		return this[index];
	}

	*[Symbol.iterator](): IterableIterator<Touch> {
		for (let i = 0; i < this.length; i++) {
			yield this[i];
		}
	}
}
def(TouchList);

function toTouchList(t: Touch[] = []): TouchList {
	const r = Object.create(TouchList.prototype);
	Object.defineProperty(r, 'length', { value: t.length, writable: false });
	Object.assign(r, t);
	return r;
}

export interface TouchEventInit extends EventModifierInit {
	changedTouches?: Touch[];
	targetTouches?: Touch[];
	touches?: Touch[];
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
		this.changedTouches = toTouchList(options.changedTouches);
		this.targetTouches = toTouchList(options.targetTouches);
		this.touches = toTouchList(options.touches);
	}
}

export interface ErrorEventInit extends EventInit {
	colno?: number;
	error?: any;
	filename?: string;
	lineno?: number;
	message?: string;
}

export class ErrorEvent extends Event implements globalThis.ErrorEvent {
	colno: number;
	error: any;
	filename: string;
	lineno: number;
	message: string;
	constructor(type: string, options: ErrorEventInit) {
		super(type, options);
		this.colno = options.colno ?? 0;
		this.error = options.error;
		this.filename = options.filename ?? '';
		this.lineno = options.lineno ?? 0;
		this.message = this.error?.message ?? '';
	}
}

export interface PromiseRejectionEventInit extends EventInit {
	promise: Promise<any>;
	reason?: any;
}

export class PromiseRejectionEvent
	extends Event
	implements globalThis.PromiseRejectionEvent
{
	promise: Promise<any>;
	reason: any;
	constructor(type: string, options: PromiseRejectionEventInit) {
		super(type, options);
		this.promise = options.promise;
		this.reason = options.reason;
	}
}

export interface GamepadEventInit extends EventInit {
	gamepad: Gamepad;
}

export class GamepadEvent extends Event implements globalThis.GamepadEvent {
	readonly gamepad: Gamepad;
	constructor(type: string, options: GamepadEventInit) {
		super(type, options);
		this.gamepad = options.gamepad;
	}
}

def(Event);
def(CustomEvent);
def(ErrorEvent);
def(PromiseRejectionEvent);
def(UIEvent);
def(KeyboardEvent);
def(TouchEvent);
def(GamepadEvent);
