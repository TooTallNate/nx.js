import { $ } from './$';
import { toPromise } from './utils';

/**
 * Performs a DNS lookup to resolve a hostname to an array of IP addresses.
 *
 * @example
 *
 * ```typescript
 * const ipAddresses = await Switch.resolveDns('example.com');
 * ```
 */
export function resolve(hostname: string) {
	return toPromise($.dnsResolve, hostname);
}
