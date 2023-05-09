import { def } from './polyfills';
import { Switch as _Switch } from './switch';
import { Console } from './console';

export type { Switch } from './switch';

const Switch = new _Switch();
def('Switch', Switch);

def('console', new Console(Switch));
