import { $ } from '../$';

/**
 * The V8 inspector, spoken to over the Chrome DevTools Protocol.
 *
 * Starting it opens a WebSocket server the same tools that debug Node attach
 * to: `chrome://inspect`, the "Open dedicated DevTools for Node" window, or an
 * editor's JavaScript debugger pointed at the console's address.
 *
 * Breakpoints stop the whole isolate, which is why the transport is native and
 * not written on top of `Switch.listen()`: while execution is paused no
 * JavaScript runs, so nothing written in JavaScript could receive the message
 * that resumes it.
 */
export interface InspectorOptions {
	/** Defaults to 9229, the port Node uses. */
	port?: number;
	/**
	 * Block until a debugger has attached and told the app to continue.
	 *
	 * This is the only way to break on something that happens during startup;
	 * without it the app has usually run past the interesting part before a
	 * debugger can connect.
	 */
	wait?: boolean;
}

/**
 * Starts the inspector and returns the port it is listening on.
 *
 * Safe to call more than once; later calls return the port already in use.
 */
export function start(opts: InspectorOptions = {}): number {
	return $.inspectorStart(opts);
}

/** Closes the inspector, disconnecting any attached debugger. */
export function stop(): void {
	$.inspectorStop();
}

/** Whether a debugger is currently attached. */
export function attached(): boolean {
	return $.inspectorAttached();
}
