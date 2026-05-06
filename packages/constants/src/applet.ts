/**
 * The category of applet that an nx.js process is running as. Returned by
 * `appletGetAppletType()`.
 *
 * Most homebrew launched from hbmenu (without a title override) runs as
 * `LibraryApplet`. Installed NSPs and title-override hbmenu launches run as
 * `Application`. Some Switch.* APIs (notably [`Switch.WebApplet`](/runtime/api/namespaces/Switch/classes/WebApplet))
 * require `Application` mode.
 */
export enum AppletType {
	/** No applet type — the runtime is not running as a regular applet. */
	None = -2,
	/** Default — same as the value returned by libnx for an unspecified applet type. */
	Default = -1,
	/** Running as the foreground Application (an installed game/app). */
	Application = 0,
	/** Running as the system applet (e.g. the home menu). */
	SystemApplet = 1,
	/** Running as a Library Applet (e.g. hbmenu without a title override, or the on-screen keyboard). */
	LibraryApplet = 2,
	/** Running as an Overlay Applet (e.g. the screenshot/album overlay). */
	OverlayApplet = 3,
	/** Running as a SystemApplication (background system service). */
	SystemApplication = 4,
}

/**
 * Whether the console is running in handheld mode or docked. Tracked by the
 * `screen.mode` accessor and the `screenmodechange` event.
 */
export enum OperationMode {
	/** The console is undocked / portable mode. */
	Handheld = 0,
	/** The console is docked / TV-mode. */
	Console = 1,
}
