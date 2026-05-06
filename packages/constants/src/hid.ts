/**
 * Bitmask values for buttons reported by the HID NPad service.
 *
 * **Note:** This is the libnx ordering and is **not** the same as the
 * Web Gamepad API button order. When working with the standard
 * `Gamepad.buttons[]` array, prefer the
 * [`Button`](/constants/api/enumerations/Button) enum.
 */
export enum HidNpadButton {
	/** A button (right face button on a Pro Controller / Joy-Con grip). */
	A = 1 << 0,
	/** B button (down face button). */
	B = 1 << 1,
	/** X button (up face button). */
	X = 1 << 2,
	/** Y button (left face button). */
	Y = 1 << 3,
	/** Click of the Left Stick. */
	StickL = 1 << 4,
	/** Click of the Right Stick. */
	StickR = 1 << 5,
	/** L shoulder button. */
	L = 1 << 6,
	/** R shoulder button. */
	R = 1 << 7,
	/** ZL trigger. */
	ZL = 1 << 8,
	/** ZR trigger. */
	ZR = 1 << 9,
	/** Plus (+) button. */
	Plus = 1 << 10,
	/** Minus (–) button. */
	Minus = 1 << 11,
	/** D-Pad Left. */
	Left = 1 << 12,
	/** D-Pad Up. */
	Up = 1 << 13,
	/** D-Pad Right. */
	Right = 1 << 14,
	/** D-Pad Down. */
	Down = 1 << 15,
	/** Pseudo-button: Left Stick tilted Left past the deadzone. */
	StickLLeft = 1 << 16,
	/** Pseudo-button: Left Stick tilted Up past the deadzone. */
	StickLUp = 1 << 17,
	/** Pseudo-button: Left Stick tilted Right past the deadzone. */
	StickLRight = 1 << 18,
	/** Pseudo-button: Left Stick tilted Down past the deadzone. */
	StickLDown = 1 << 19,
	/** Pseudo-button: Right Stick tilted Left past the deadzone. */
	StickRLeft = 1 << 20,
	/** Pseudo-button: Right Stick tilted Up past the deadzone. */
	StickRUp = 1 << 21,
	/** Pseudo-button: Right Stick tilted Right past the deadzone. */
	StickRRight = 1 << 22,
	/** Pseudo-button: Right Stick tilted Down past the deadzone. */
	StickRDown = 1 << 23,
	/** SL button on a Left Joy-Con (when held sideways). */
	LeftSL = 1 << 24,
	/** SR button on a Left Joy-Con (when held sideways). */
	LeftSR = 1 << 25,
	/** SL button on a Right Joy-Con (when held sideways). */
	RightSL = 1 << 26,
	/** SR button on a Right Joy-Con (when held sideways). */
	RightSR = 1 << 27,
	/** Top button on the Poké Ball Plus (Palma) controller. */
	Palma = 1 << 28,
	/** Internal "verification" button used during HID input testing. */
	Verification = 1 << 29,
	/** B button on a Left NES/HVC controller in Handheld mode. */
	HandheldLeftB = 1 << 30,
	/** Left C button on the N64 controller. */
	LagonCLeft = 1 << 31,

	// These values overflow the 32-bit bitmask — they should not be used.
	/** Up C button on the N64 controller. *(Overflows 32-bit; do not use.)* */
	LagonCUp = 1 << 0 /*1 << 32*/,
	/** Right C button on the N64 controller. *(Overflows 32-bit; do not use.)* */
	LagonCRight = 1 << 1 /*1 << 33*/,
	/** Down C button on the N64 controller. *(Overflows 32-bit; do not use.)* */
	LagonCDown = 1 << 2 /*1 << 34*/,

	/** Bitmask of all buttons that count as "Left" (D-Pad and either stick). */
	AnyLeft = (1 << 12) | (1 << 16) | (1 << 20),
	/** Bitmask of all buttons that count as "Up" (D-Pad and either stick). */
	AnyUp = (1 << 13) | (1 << 17) | (1 << 21),
	/** Bitmask of all buttons that count as "Right" (D-Pad and either stick). */
	AnyRight = (1 << 14) | (1 << 18) | (1 << 22),
	/** Bitmask of all buttons that count as "Down" (D-Pad and either stick). */
	AnyDown = (1 << 15) | (1 << 19) | (1 << 23),
	/** Bitmask matching SL on either Joy-Con. */
	AnySL = (1 << 24) | (1 << 26),
	/** Bitmask matching SR on either Joy-Con. */
	AnySR = (1 << 25) | (1 << 27),
}

/**
 * Bitmask identifying the physical kind of a HID device. Reported by
 * the system HID service.
 */
export enum HidDeviceTypeBits {
	/** Pro Controller or GameCube controller. */
	FullKey = 1 << 0,
	/** DebugPad (development hardware). */
	DebugPad = 1 << 1,
	/** Joy-Con / Famicom / NES left controller in handheld mode. */
	HandheldLeft = 1 << 2,
	/** Joy-Con / Famicom / NES right controller in handheld mode. */
	HandheldRight = 1 << 3,
	/** Joy-Con left controller (detached). */
	JoyLeft = 1 << 4,
	/** Joy-Con right controller (detached). */
	JoyRight = 1 << 5,
	/** Poké Ball Plus controller. */
	Palma = 1 << 6,
	/** Famicom left controller. */
	LarkHvcLeft = 1 << 7,
	/** Famicom right controller (with microphone). */
	LarkHvcRight = 1 << 8,
	/** NES left controller. */
	LarkNesLeft = 1 << 9,
	/** NES right controller. */
	LarkNesRight = 1 << 10,
	/** Famicom left controller in handheld mode. */
	HandheldLarkHvcLeft = 1 << 11,
	/** Famicom right controller (with microphone) in handheld mode. */
	HandheldLarkHvcRight = 1 << 12,
	/** NES left controller in handheld mode. */
	HandheldLarkNesLeft = 1 << 13,
	/** NES right controller in handheld mode. */
	HandheldLarkNesRight = 1 << 14,
	/** SNES controller. */
	Lucia = 1 << 15,
	/** N64 controller. */
	Lagon = 1 << 16,
	/** Sega Genesis controller. */
	Lager = 1 << 17,
	/** Generic system controller. */
	System = 1 << 31,
}

/**
 * Bitmask identifying the controller "style" reported by the NPad service.
 * A single physical device can correspond to multiple styles depending on
 * how it's held (e.g. a Joy-Con pair shows up as both `JoyDual` and `JoyLeft`/`JoyRight`).
 */
export enum HidNpadStyleTag {
	/** Pro Controller. */
	FullKey = 1 << 0,
	/** Joy-Con(s) attached to the console in handheld mode. */
	Handheld = 1 << 1,
	/** Joy-Con pair held in dual-grip mode. */
	JoyDual = 1 << 2,
	/** Single Left Joy-Con held sideways. */
	JoyLeft = 1 << 3,
	/** Single Right Joy-Con held sideways. */
	JoyRight = 1 << 4,
	/** GameCube controller. */
	Gc = 1 << 5,
	/** Poké Ball Plus controller. */
	Palma = 1 << 6,
	/** NES / Famicom controller. */
	Lark = 1 << 7,
	/** NES / Famicom controller in handheld mode. */
	HandheldLark = 1 << 8,
	/** SNES controller. */
	Lucia = 1 << 9,
	/** N64 controller. */
	Lagon = 1 << 10,
	/** Sega Genesis controller. */
	Lager = 1 << 11,
	/** Generic external controller (extension API). */
	SystemExt = 1 << 29,
	/** Generic controller. */
	System = 1 << 30,
}
