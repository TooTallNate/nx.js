import 'core-js/actual/url';
import 'core-js/actual/url-search-params';
import EventTarget from "@ungap/event-target";
import { console } from './console';

const def = (key: string, value: unknown) =>
  Object.defineProperty(globalThis, key, {
    value,
    writable: true,
    enumerable: false,
    configurable: true,
  });

def("EventTarget", EventTarget);

def('console', console);

const Switch: import('./types').Switch = new EventTarget();
def('Switch', Switch);