/// AppletType
export enum Type {
	None = -2,
	Default = -1,
	Application = 0,
	SystemApplet = 1,
	LibraryApplet = 2,
	OverlayApplet = 3,
	SystemApplication = 4,
}

/// OperationMode
export enum OperationMode {
	Handheld = 0, ///< Handheld
	Console = 1, ///< Console (Docked / TV-mode)
}
