import 'core-js/actual/url';
import 'core-js/actual/url-search-params';

export declare class URL {
	constructor(url: string | URL, base?: string | URL);
	hash: string;
	host: string;
	hostname: string;
	href: string;
	toString(): string;
	origin: string;
	password: string;
	pathname: string;
	port: string;
	protocol: string;
	search: string;
	searchParams: URLSearchParams;
	username: string;
	toJSON(): string;

	static createObjectURL: (obj: Blob) => void;
	static revokeObjectURL: (url: string) => void;
};

export const objectUrls = new Map<string, Blob>();

URL.createObjectURL = (obj) => {
	if (!(obj instanceof Blob)) {
		throw new Error('Must be Blob');
	}
	const url = `blob:${crypto.randomUUID()}`;
	objectUrls.set(url, obj);
	return url;
};

URL.revokeObjectURL = (url) => {
	objectUrls.delete(url);
};
