/// HidNpadButton
export enum Button {
	A = 1 << 0, ///< A button / Right face button
	B = 1 << 1, ///< B button / Down face button
	X = 1 << 2, ///< X button / Up face button
	Y = 1 << 3, ///< Y button / Left face button
	StickL = 1 << 4, ///< Left Stick button
	StickR = 1 << 5, ///< Right Stick button
	L = 1 << 6, ///< L button
	R = 1 << 7, ///< R button
	ZL = 1 << 8, ///< ZL button
	ZR = 1 << 9, ///< ZR button
	Plus = 1 << 10, ///< Plus button
	Minus = 1 << 11, ///< Minus button
	Left = 1 << 12, ///< D-Pad Left button
	Up = 1 << 13, ///< D-Pad Up button
	Right = 1 << 14, ///< D-Pad Right button
	Down = 1 << 15, ///< D-Pad Down button
	StickLLeft = 1 << 16, ///< Left Stick pseudo-button when moved Left
	StickLUp = 1 << 17, ///< Left Stick pseudo-button when moved Up
	StickLRight = 1 << 18, ///< Left Stick pseudo-button when moved Right
	StickLDown = 1 << 19, ///< Left Stick pseudo-button when moved Down
	StickRLeft = 1 << 20, ///< Right Stick pseudo-button when moved Left
	StickRUp = 1 << 21, ///< Right Stick pseudo-button when moved Up
	StickRRight = 1 << 22, ///< Right Stick pseudo-button when moved Right
	StickRDown = 1 << 23, ///< Right Stick pseudo-button when moved Left
	LeftSL = 1 << 24, ///< SL button on Left Joy-Con
	LeftSR = 1 << 25, ///< SR button on Left Joy-Con
	RightSL = 1 << 26, ///< SL button on Right Joy-Con
	RightSR = 1 << 27, ///< SR button on Right Joy-Con
	Palma = 1 << 28, ///< Top button on Poké Ball Plus (Palma) controller
	Verification = 1 << 29, ///< Verification
	HandheldLeftB = 1 << 30, ///< B button on Left NES/HVC controller in Handheld mode
	LagonCLeft = 1 << 31, ///< Left C button in N64 controller
	LagonCUp = 1 << 32, ///< Up C button in N64 controller
	LagonCRight = 1 << 33, ///< Right C button in N64 controller
	LagonCDown = 1 << 34, ///< Down C button in N64 controller

	AnyLeft = (1 << 12) | (1 << 16) | (1 << 20), ///< Bitmask containing all buttons that are considered Left (D-Pad, Sticks)
	AnyUp = (1 << 13) | (1 << 17) | (1 << 21), ///< Bitmask containing all buttons that are considered Up (D-Pad, Sticks)
	AnyRight = (1 << 14) | (1 << 18) | (1 << 22), ///< Bitmask containing all buttons that are considered Right (D-Pad, Sticks)
	AnyDown = (1 << 15) | (1 << 19) | (1 << 23), ///< Bitmask containing all buttons that are considered Down (D-Pad, Sticks)
	AnySL = (1 << 24) | (1 << 26), ///< Bitmask containing SL buttons on both Joy-Cons (Left/Right)
	AnySR = (1 << 25) | (1 << 27), ///< Bitmask containing SR buttons on both Joy-Cons (Left/Right)
}
