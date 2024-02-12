import { $ } from '../$';
import { def } from '../utils';
import { Body, type BodyInit } from './body';
import { AbortController, AbortSignal } from '../polyfills/abort-controller';
import { URL } from '../polyfills/url';
import type { HeadersInit } from './headers';

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

export type RequestCache =
	| 'default'
	| 'force-cache'
	| 'no-cache'
	| 'no-store'
	| 'only-if-cached'
	| 'reload';
export type RequestCredentials = 'include' | 'omit' | 'same-origin';
export type RequestDestination =
	| ''
	| 'audio'
	| 'audioworklet'
	| 'document'
	| 'embed'
	| 'font'
	| 'frame'
	| 'iframe'
	| 'image'
	| 'manifest'
	| 'object'
	| 'paintworklet'
	| 'report'
	| 'script'
	| 'sharedworker'
	| 'style'
	| 'track'
	| 'video'
	| 'worker'
	| 'xslt';
export type RequestMode = 'cors' | 'navigate' | 'no-cors' | 'same-origin';
export type RequestRedirect = 'error' | 'follow' | 'manual';

export interface RequestInit {
	/** A BodyInit object or null to set request's body. */
	body?: BodyInit | null;
	/** A string indicating how the request will interact with the local cache to set request's cache. */
	cache?: RequestCache;
	/** A string indicating whether credentials will be sent with the request always, never, or only when sent to a same-origin URL. Sets request's credentials. */
	credentials?: RequestCredentials;
	/** A Headers object, an object literal, or an array of two-item arrays to set request's headers. */
	headers?: HeadersInit;
	/** A cryptographic hash of the resource to be fetched by request. Sets request's integrity. */
	integrity?: string;
	/** A boolean to set request's keepalive. */
	keepalive?: boolean;
	/** A string to set request's method. */
	method?: string;
	/** A string to indicate whether the request will use CORS, or will be restricted to same-origin URLs. Sets request's mode. */
	mode?: RequestMode;
	/** A string indicating whether request follows redirects, results in an error upon encountering a redirect, or returns the redirect (in an opaque fashion). Sets request's redirect. */
	redirect?: RequestRedirect;
	/** A string whose value is a same-origin URL, "about:client", or the empty string, to set request's referrer. */
	referrer?: string;
	/** A referrer policy to set request's referrerPolicy. */
	referrerPolicy?: ReferrerPolicy;
	/** An AbortSignal to set request's signal. */
	signal?: AbortSignal | null;
	/** Can only be null. Used to disassociate request from any Window. */
	window?: null;
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
					? new URL(input, $.entrypoint)
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

def(Request);
