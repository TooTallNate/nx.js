import { def } from './polyfills';
import { Switch as _Switch, INTERNAL_SYMBOL } from './switch';
import { createTimersFactory } from './timers';
import { Console } from './console';

export type { Switch } from './switch';

const Switch = new _Switch();
def('Switch', Switch);

def('console', new Console(Switch));

const { setTimeout, setInterval, clearTimeout, clearInterval, processTimers } =
	createTimersFactory(Switch);
def('setTimeout', setTimeout);
def('setInterval', setInterval);
def('clearTimeout', clearTimeout);
def('clearInterval', clearInterval);

Switch.addEventListener('frame', () => {
	processTimers();
});

Switch.addEventListener('exit', () => {
	Switch[INTERNAL_SYMBOL].cleanup();
});
