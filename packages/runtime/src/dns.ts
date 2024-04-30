import { $ } from './$';

/**
 * Performs a DNS lookup to resolve a hostname to an array of IP addresses.
 *
 * @example
 *
 * ```typescript
 * const ipAddresses = await Switch.resolveDns('example.com');
 * ```
 */
export function resolveDns(hostname: string) {
	return $.dnsResolve(hostname);
}
