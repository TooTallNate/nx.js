import { $ } from '../$';
import { def } from '../utils';
import { Body, type BodyInit } from './body';
import { AbortSignal } from '../polyfills/abort-controller';
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

// Shared "never abort" signal — avoids creating a new AbortController per Request
const neverAbortSignal = Object.freeze(new AbortSignal());

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

/**
 * Options accepted by {@link fetch | `fetch()`} and the `Request` constructor.
 *
 * The shape mirrors the Web Fetch standard, but **not every field has an
 * equivalent on the Switch runtime**. There is no Window, no same-origin
 * policy, no CORS enforcement, and no shared HTTP cache. The notes on each
 * field below describe how nx.js actually treats them.
 *
 * @see {@link fetch}
 */
export interface RequestInit {
	/** Honored — the request body. May be a `BodyInit` (string, `ArrayBuffer`, `FormData`, `URLSearchParams`, `Blob`, or `ReadableStream`) or `null`. */
	body?: BodyInit | null;
	/**
	 * **Ignored on Switch.** nx.js does not maintain an HTTP cache; every
	 * `fetch()` performs a fresh network request. The value is stored on the
	 * resulting `Request` object for spec compatibility but has no effect on
	 * networking behavior.
	 */
	cache?: RequestCache;
	/**
	 * **Ignored on Switch.** There is no shared cookie jar or credential
	 * store, and there is no same-origin policy to gate. The value is
	 * preserved on the `Request` for spec compatibility only.
	 */
	credentials?: RequestCredentials;
	/** Honored — request headers (`Headers`, an object literal, or an array of `[name, value]` pairs). */
	headers?: HeadersInit;
	/**
	 * **Not enforced on Switch.** Subresource integrity is not validated by
	 * the runtime; the value is stored on the `Request` but no hash check is
	 * performed.
	 */
	integrity?: string;
	/**
	 * **Ignored on Switch.** There is no concept of a page lifetime that
	 * `keepalive` would extend a request beyond.
	 */
	keepalive?: boolean;
	/** Honored — HTTP method (`'GET'`, `'POST'`, etc.). Defaults to `'GET'`. */
	method?: string;
	/**
	 * **Ignored on Switch.** There is no browser to enforce CORS / same-origin
	 * checks. All cross-origin requests are simply performed. The value is
	 * preserved on the `Request` for spec compatibility.
	 */
	mode?: RequestMode;
	/**
	 * Honored — controls how 3xx redirects are handled (`'follow'` (default),
	 * `'error'`, or `'manual'`).
	 */
	redirect?: RequestRedirect;
	/** **Ignored on Switch.** No `Referer` header is set automatically; supply one via `headers` if needed. */
	referrer?: string;
	/** **Ignored on Switch.** Referrer policy is a browser concept and has no effect here. */
	referrerPolicy?: ReferrerPolicy;
	/** Honored — an `AbortSignal` that aborts the in-flight request when triggered. */
	signal?: AbortSignal | null;
	/** **Ignored on Switch** — there is no `Window` to disassociate from. Kept only for spec compatibility. */
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
			if (input.bodyUsed) {
				throw new TypeError('Already read');
			}
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
			this.url = input.url;
			this.cache = init.cache || input.cache;
			this.credentials = init.credentials ?? input.credentials;
			this.destination = input.destination;
			this.integrity = init.integrity ?? input.integrity;
			this.keepalive = init.keepalive ?? input.keepalive;
			this.mode = init.mode ?? input.mode;
			this.redirect = init.redirect ?? input.redirect;
			this.referrer = init.referrer ?? input.referrer;
			this.referrerPolicy = init.referrerPolicy ?? input.referrerPolicy;
			this.signal = init.signal ?? input.signal;
		} else {
			const url =
				typeof input === 'string' ? new URL(input, $.entrypoint) : input;
			this.url = url.href;
			this.cache = init.cache || 'default';
			this.credentials = init.credentials || 'same-origin';
			this.destination = '';
			this.integrity = init.integrity ?? '';
			this.keepalive = init.keepalive ?? false;
			this.mode = init.mode ?? 'cors';
			this.redirect = init.redirect ?? 'follow';
			this.referrer = init.referrer ?? '';
			this.referrerPolicy = init.referrerPolicy ?? '';
			this.signal = init.signal ?? neverAbortSignal;
		}
		this.method = method;
	}

	clone(): Request {
		if (this.bodyUsed) {
			throw new TypeError('Already read');
		}
		// Tee the body stream so both the original and clone can be read
		let cloneBody: BodyInit | null = null;
		if (this.body instanceof ReadableStream) {
			const [branch1, branch2] = this.body.tee();
			this.body = branch1;
			cloneBody = branch2;
		} else {
			cloneBody = this.body;
		}
		const cloned = new Request(this.url, {
			method: this.method,
			headers: this.headers,
			body: cloneBody,
			cache: this.cache,
			credentials: this.credentials,
			integrity: this.integrity,
			keepalive: this.keepalive,
			mode: this.mode,
			redirect: this.redirect,
			referrer: this.referrer,
			referrerPolicy: this.referrerPolicy,
			signal: this.signal,
		});
		return cloned;
	}
}

def(Request);
