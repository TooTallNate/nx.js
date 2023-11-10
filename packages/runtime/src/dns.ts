import { $ } from './$';
import { toPromise } from './utils';

export function resolve(hostname: string) {
	return toPromise($.dnsResolve, hostname);
}
