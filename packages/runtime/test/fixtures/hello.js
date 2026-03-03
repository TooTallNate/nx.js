// Simple test to verify the nxjs-test binary works
console.log('Hello from nxjs-test!');
console.log('Switch.version:', Switch.version);
console.log('Switch.entrypoint:', Switch.entrypoint);
console.log('Switch.argv:', Switch.argv);
console.log('Switch.cwd():', Switch.cwd());
console.log('typeof URL:', typeof URL);
console.log('typeof OffscreenCanvas:', typeof OffscreenCanvas);
console.log('typeof crypto:', typeof crypto);
console.log('typeof WebAssembly:', typeof WebAssembly);
console.log('typeof TextEncoder:', typeof TextEncoder);
console.log('typeof TextDecoder:', typeof TextDecoder);
console.log('typeof setTimeout:', typeof setTimeout);
console.log('typeof fetch:', typeof fetch);

// Test URL parsing
const url = new URL('https://example.com/path?key=value#hash');
console.log(
	'URL parsed:',
	url.hostname,
	url.pathname,
	url.searchParams.get('key'),
);

// Test crypto.randomUUID
console.log('UUID:', crypto.randomUUID());

// Test TextEncoder/TextDecoder
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const encoded = encoder.encode('Hello, World!');
const decoded = decoder.decode(encoded);
console.log('TextEncoder/TextDecoder:', decoded);

const res = await fetch('https://jsonip.com');
console.log(await res.json());
console.log(Switch.Profile.select());

// Test setTimeout (async)
setTimeout(() => {
	console.log('setTimeout fired!');

	// Exit after the test
	Switch.exit();
}, 100);
