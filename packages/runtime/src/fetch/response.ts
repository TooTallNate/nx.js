import { def } from '../utils';
import { Body } from './body';

export class Response extends Body implements globalThis.Response {
	redirected: boolean;
	status: number;
	statusText: string;
	type: ResponseType;
	url: string;

	constructor(body?: BodyInit | null, init: ResponseInit = {}) {
		super(body, init.headers);
		this.redirected = false;
		this.status = init.status ?? 200;
		this.statusText = init.statusText ?? '';
		this.type = 'default';
		this.url = '';
	}

	get ok(): boolean {
		return this.status >= 200 && this.status < 300;
	}

	clone(): Response {
		throw new Error('Method not implemented.');
	}

	static error(): Response {
		const res = new Response();
		res.status = 0;
		res.type = 'error';
		return res;
	}

	static redirect(url: string | URL, status = 302): Response {
		return new Response(null, {
			status,
			headers: {
				location: String(url),
			},
		});
	}

	static json(data: any, init: ResponseInit = {}): Response {
		const headers = new Headers(init.headers);
		if (!headers.has('content-type')) {
			headers.set('content-type', 'application/json');
		}
		return new Response(JSON.stringify(data), { ...init, headers });
	}
}
def('Response', Response);
