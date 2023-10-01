interface Timer {
	args: any[];
	callback: Function;
	invokeAt: number;
	interval?: number;
}

/**
 * Represents a handler for a timer. Can be a string or a function.
 */
export type TimerHandler = string | Function;

let nextId = 0;
const timers = new Map<number, Timer>();

/**
 * The global `setTimeout()` method sets a timer which executes a function or specified piece of code once the timer expires.
 *
 * @see https://developer.mozilla.org/docs/Web/API/setTimeout
 * @param handler - The function or string to be executed after the timer expires.
 * @param timeout - The time, in milliseconds, the timer should wait before the specified function or code is executed. If this parameter is omitted, a value of 0 is used.
 * @param args - Additional arguments to be passed to the function specified by the handler parameter.
 * @returns The numeric ID of the timer, which can be used later with the {@link clearTimeout | `clearTimeout()`} method to cancel the timer.
 */
export function setTimeout(handler: TimerHandler, timeout = 0, ...args: any[]) {
	const id = ++nextId;
	const callback =
		typeof handler === 'string' ? new Function(handler) : handler;
	timers.set(id, {
		args,
		callback,
		invokeAt: Date.now() + timeout,
	});
	return id;
}

/**
 * The global `setInterval()` method repeatedly calls a function or executes a code snippet, with a fixed time delay between each call.
 *
 * @see https://developer.mozilla.org/docs/Web/API/setInterval
 * @param handler - The function or string to be executed every time the interval elapses.
 * @param timeout - The time, in milliseconds, the timer should delay in between executions of the specified function or code. If this parameter is omitted, a value of 0 is used.
 * @param args - Additional arguments to be passed to the function specified by the handler parameter.
 * @returns The numeric ID of the timer, which can be used later with the {@link clearInterval | `clearInterval()`} method to cancel the timer.
 */
export function setInterval(
	handler: TimerHandler,
	timeout = 0,
	...args: any[]
) {
	const id = ++nextId;
	const callback =
		typeof handler === 'string' ? new Function(handler) : handler;
	timers.set(id, {
		args,
		callback,
		invokeAt: Date.now() + timeout,
		interval: timeout,
	});
	return id;
}

/**
 * The global `clearTimeout()` method clears a timer set with the {@link setTimeout | `setTimeout()`} method.
 *
 * @see https://developer.mozilla.org/docs/Web/API/clearTimeout
 * @param id - The ID of the timer you want to clear, as returned by {@link setTimeout | `setTimeout()`}.
 */
export function clearTimeout(id: number) {
	timers.delete(id);
}

/**
 * The global `clearInterval()` method clears a timer set with the {@link setInterval | `setInterval()`} method.
 *
 * @see https://developer.mozilla.org/docs/Web/API/clearInterval
 * @param id - The ID of the timer you want to clear, as returned by {@link setInterval | `setInterval()`}.
 */
export function clearInterval(id: number) {
	timers.delete(id);
}

export function processTimers() {
	const now = Date.now();
	for (const [id, timer] of timers) {
		if (now >= timer.invokeAt) {
			timer.callback.apply(null, timer.args);
			if (typeof timer.interval === 'number') {
				timer.invokeAt = now + timer.interval;
			} else {
				clearTimeout(id);
			}
		}
	}
}
