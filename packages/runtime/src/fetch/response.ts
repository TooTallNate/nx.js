import { def } from '../utils';
import { Body, type BodyInit } from './body';
import { Headers, type HeadersInit } from './headers';

/**
 * Interface for the initialization options for a Response.
 */
export interface ResponseInit {
	/** Headers for the response. */
	headers?: HeadersInit;
	/** HTTP status code for the response. */
	status?: number;
	/** Status text for the response. */
	statusText?: string;
}

/**
 * Type for the response type.
 */
export type ResponseType = 'default' | 'error';

/**
 * Class representing a HTTP response.
 */
export class Response extends Body implements globalThis.Response {
	/** Whether the response was redirected. */
	redirected: boolean;
	/** HTTP status code of the response. */
	status: number;
	/** Status text of the response. */
	statusText: string;
	/** Type of the response. */
	type: ResponseType;
	/** URL of the response. */
	url: string;

	/**
	 * Create a new Response.
	 * @param body - The body of the response.
	 * @param init - Initialization options for the response.
	 */
	constructor(body?: BodyInit | null, init: ResponseInit = {}) {
		super(body, init.headers);
		this.redirected = false;
		this.status = init.status ?? 200;
		this.statusText = init.statusText ?? '';
		this.type = 'default';
		this.url = '';
	}

	/**
	 * Check if the response was successful.
	 * @returns {boolean} - True if the status is between 200 and 299, inclusive.
	 */
	get ok(): boolean {
		return this.status >= 200 && this.status < 300;
	}

	/**
	 * Clone the response.
	 * @throws {Error} - This method is not implemented.
	 */
	clone(): Response {
		throw new Error('Method not implemented.');
	}

	/**
	 * Create a new error response.
	 * @returns {Response} - The new error response.
	 */
	static error(): Response {
		const res = new Response();
		res.status = 0;
		res.type = 'error';
		return res;
	}

	/**
	 * Create a new redirect response.
	 * @param {string | URL} url - The URL to redirect to.
	 * @param {number} status - The status code for the redirect, defaults to 302.
	 * @returns {Response} - The new redirect response.
	 */
	static redirect(url: string | URL, status = 302): Response {
		return new Response(null, {
			status,
			headers: {
				location: String(url),
			},
		});
	}

	/**
	 * Create a new JSON response.
	 * @param {any} data - The data to include in the response body.
	 * @param {ResponseInit} init - Initialization options for the response.
	 * @returns {Response} - The new JSON response.
	 */
	static json(data: any, init: ResponseInit = {}): Response {
		const headers = new Headers(init.headers);
		if (!headers.has('content-type')) {
			headers.set('content-type', 'application/json');
		}
		return new Response(JSON.stringify(data), { ...init, headers });
	}
}
def('Response', Response);
