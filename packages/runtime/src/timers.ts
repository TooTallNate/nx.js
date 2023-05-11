import type { Switch } from './switch';

interface Timer {
	args: any[];
	callback: Function;
	invokeAt: number;
	interval?: number;
}

export function createTimersFactory(s: Switch) {
	let nextId = 0;
	const timers = new Map<number, Timer>();

	function setTimeout(handler: TimerHandler, timeout = 0, ...args: any[]) {
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

	function setInterval(handler: TimerHandler, timeout = 0, ...args: any[]) {
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

	function clearTimeout(id: number) {
		timers.delete(id);
	}

	function clearInterval(id: number) {
		timers.delete(id);
	}

	function processTimers() {
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

	return {
		setTimeout,
		setInterval,
		clearTimeout,
		clearInterval,
		processTimers,
	};
}
