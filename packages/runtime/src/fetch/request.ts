import { def } from '../utils';
import { Body } from './body';
import { AbortSignal } from '../polyfills/abort-controller';
import type { Switch as _Switch } from '../switch';

declare const Switch: _Switch;

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

function normalizeMethod(method: string) {
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

	constructor(input: RequestInfo | URL, init: RequestInit = {}) {
		super(
			init.body ?? (input instanceof globalThis.Request ? input : null),
			init.headers ??
				(input instanceof globalThis.Request
					? input.headers
					: undefined)
		);

		if (input instanceof globalThis.Request) {
			if (input.bodyUsed) {
				throw new TypeError('Already read');
			}
			this.url = input.url;
			this.cache = init.cache || input.cache;
			this.credentials = input.credentials;
			this.destination = input.destination;
			this.integrity = init.integrity ?? input.integrity;
			this.keepalive = init.keepalive ?? input.keepalive;
			this.method = init.method ?? input.method;
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
			this.method = normalizeMethod(init.method || 'GET');
			this.mode = init.mode ?? 'cors';
			this.redirect = init.redirect ?? 'follow';
			this.referrerPolicy = init.referrerPolicy ?? '';
			this.signal = init.signal ?? new AbortController().signal;
		}
		this.referrer = '';

		//if ((this.method === 'GET' || this.method === 'HEAD') && body) {
		//	throw new TypeError('Body not allowed for GET or HEAD requests');
		//}
		//this._initBody(body);

		//if (this.method === 'GET' || this.method === 'HEAD') {
		//	if (init.cache === 'no-store' || init.cache === 'no-cache') {
		//		// Search for a '_' parameter in the query string
		//		var reParamSearch = /([?&])_=[^&]*/;
		//		if (reParamSearch.test(this.url)) {
		//			// If it already exists then set the value with the current time
		//			this.url = this.url.replace(
		//				reParamSearch,
		//				'$1_=' + new Date().getTime()
		//			);
		//		} else {
		//			// Otherwise add a new '_' parameter to the end with the current time
		//			var reQueryString = /\?/;
		//			this.url +=
		//				(reQueryString.test(this.url) ? '&' : '?') +
		//				'_=' +
		//				new Date().getTime();
		//		}
		//	}
		//}
	}

	clone(): Request {
		throw new Error('not implemented');
		//return new Request(this, { body: this._bodyInit });
	}
}

def('Request', Request);
