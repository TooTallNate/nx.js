import 'core-js/actual/url';
import 'core-js/actual/url-search-params';
import EventTarget from "@ungap/event-target";
import { Switch as _Switch } from './switch';
import { console } from './console';

export type { Switch } from './switch';

const def = (key: string, value: unknown) =>
  Object.defineProperty(globalThis, key, {
    value,
    writable: true,
    enumerable: false,
    configurable: true,
  });

def("EventTarget", EventTarget);

def('console', console);

const Switch = new _Switch();
def('Switch', Switch);