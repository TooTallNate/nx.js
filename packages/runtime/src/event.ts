import { INTERNAL_SYMBOL } from './types';

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
	initEvent(): void {
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
	NumPadDivide = 84,
	NumPadMultiply = 85,
	NumPadSubtract = 86,
	NumPadAdd = 87,
	NumPadEnter = 88,
	NumPad1 = 89,
	NumPad2 = 90,
	NumPad3 = 91,
	NumPad4 = 92,
	NumPad5 = 93,
	NumPad6 = 94,
	NumPad7 = 95,
	NumPad8 = 96,
	NumPad9 = 97,
	NumPad0 = 98,
	NumPadDot = 99,
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
	[KeyboardKey.Backslash, ['\'', '|']],
	[KeyboardKey.Tilde, '~'],
	[KeyboardKey.Semicolon, [';', ':']],
	[KeyboardKey.Quote, ["'", '"']],
	[KeyboardKey.Backquote, ['`', '~']],
	[KeyboardKey.Comma, [',', '<']],
	[KeyboardKey.Period, ['.', '>']],
	[KeyboardKey.Slash, ['/', '?']],
	[KeyboardKey.ControlLeft, 'Control'],
	[KeyboardKey.ShiftLeft, 'Shift'],
	[KeyboardKey.AltLeft, 'Alt'],
	[KeyboardKey.OSLeft, 'Meta'],
	[KeyboardKey.ControlRight, 'Control'],
	[KeyboardKey.ShiftRight, 'Shift'],
	[KeyboardKey.AltRight, 'Alt'],
	[KeyboardKey.OSRight, 'Meta'],
]);

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

	// modifiers
	[INTERNAL_SYMBOL]: bigint;

	constructor(type: string, options?: KeyboardEventInit) {
		super(type, options);
		if (options) {
			this.charCode = options.charCode ?? -1;
			this.isComposing = options.isComposing ?? false;
			this.keyCode = options.keyCode ?? -1;
			this.location = options.location ?? -1;
			this.repeat = options.repeat ?? false;
			// @ts-expect-error
			this[INTERNAL_SYMBOL] = options.modifiers;
		} else {
			this.charCode = this.keyCode = this.location = -1;
			this.isComposing = this.repeat = false;
			this[INTERNAL_SYMBOL] = 0n;
		}
	}

	getModifierState(): boolean {
		throw new Error('Method not implemented.');
	}

	initKeyboardEvent(): void {
		throw new Error('Method not implemented.');
	}

	get ctrlKey(): boolean {
		return (this[INTERNAL_SYMBOL] & CTRL) !== 0n;
	}

	get shiftKey(): boolean {
		return (this[INTERNAL_SYMBOL] & SHIFT) !== 0n;
	}

	get altKey(): boolean {
		return (this[INTERNAL_SYMBOL] & ALT) !== 0n;
	}

	get metaKey(): boolean {
		return (this[INTERNAL_SYMBOL] & META) !== 0n;
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
			console.log(`Invalid keyCode: ${this.keyCode}`);
			return '';
		}
		if (code.length === 1) {
			// one of the alphabetic keys
			return this.shiftKey ? code : code.toLowerCase();
		}
		return code;
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
