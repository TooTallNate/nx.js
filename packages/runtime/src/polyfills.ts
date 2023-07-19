export const def = (key: string, value: unknown) =>
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: false,
		configurable: true,
	});

import 'core-js/actual/url';
import 'core-js/actual/url-search-params';

import EventTarget from '@ungap/event-target';
def('EventTarget', EventTarget);

import {
	Event,
	ErrorEvent,
	UIEvent,
	KeyboardEvent,
	TouchEvent,
} from './polyfills/event';
def('Event', Event);
def('ErrorEvent', ErrorEvent);
def('UIEvent', UIEvent);
def('KeyboardEvent', KeyboardEvent);
def('TouchEvent', TouchEvent);

import { Blob } from './polyfills/blob';
def('Blob', Blob);

import { TextDecoder } from './polyfills/text-decoder';
def('TextDecoder', TextDecoder);

import { TextEncoder } from './polyfills/text-encoder';
def('TextEncoder', TextEncoder);
