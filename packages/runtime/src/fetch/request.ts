import { def } from '../utils';
import { Body } from './body';
import { AbortSignal } from '../polyfills/abort-controller';
import type { SwitchClass } from '../switch';

declare const Switch: SwitchClass;

// HTTP methods whose capitalization should be normalized
const methods = [
	'CONNECT',
	'DELETE',
	'GET',
	'HEAD',
	'OPTIONS',
	'PATCH',
	'POST',
	'PUT',
	'TRACE',
];

function normalizeMethod(method?: string) {
	if (!method) return;
	var upcased = method.toUpperCase();
	return methods.includes(upcased) ? upcased : method;
}

export class Request extends Body implements globalThis.Request {
	cache: RequestCache;
	credentials: RequestCredentials;
	destination: RequestDestination;
	integrity: string;
	keepalive: boolean;
	method: string;
	mode: RequestMode;
	redirect: RequestRedirect;
	referrer: string;
	referrerPolicy: ReferrerPolicy;
	signal: AbortSignal;
	url: string;

	constructor(input: string | URL | Request, init: RequestInit = {}) {
		let body: Body | BodyInit | null = init.body ?? null;
		let headers = init.headers;
		let method = normalizeMethod(init.method);
		if (input instanceof Request) {
			if (!body) body = input;
			if (!headers) headers = input.headers;
			if (!method) method = input.method;
		}
		if (!method) method = 'GET';

		if ((method === 'GET' || method === 'HEAD') && body) {
			throw new TypeError('Body not allowed for GET or HEAD requests');
		}
		super(body, headers);

		if (input instanceof Request) {
			if (input.bodyUsed) {
				throw new TypeError('Already read');
			}
			this.url = input.url;
			this.cache = init.cache || input.cache;
			this.credentials = input.credentials;
			this.destination = input.destination;
			this.integrity = init.integrity ?? input.integrity;
			this.keepalive = init.keepalive ?? input.keepalive;
			this.mode = init.mode ?? input.mode;
			this.redirect = init.redirect ?? input.redirect;
			this.referrerPolicy = init.referrerPolicy ?? input.referrerPolicy;
			this.signal = init.signal ?? input.signal;
		} else {
			const url =
				typeof input === 'string'
					? new URL(input, Switch.cwd())
					: input;
			this.url = url.href;
			this.cache = init.cache || 'default';
			this.credentials = init.credentials || 'same-origin';
			this.destination = '';
			this.integrity = init.integrity ?? '';
			this.keepalive = init.keepalive ?? false;
			this.mode = init.mode ?? 'cors';
			this.redirect = init.redirect ?? 'follow';
			this.referrerPolicy = init.referrerPolicy ?? '';
			this.signal = init.signal ?? new AbortController().signal;
		}
		this.method = method;
		this.referrer = '';
	}

	clone(): Request {
		return new Request(this);
	}
}

def('Request', Request);
