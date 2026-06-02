// Isolated network diagnostics. Runs a sequence of probes that each exercise
// ONE layer of the networking stack, capturing the real error (name + message
// + stack) to sdmc:/switch/net-debug.log so we can pinpoint which layer fails.
//
// Layers, innermost first:
//   1. DNS resolve (getaddrinfo on the worker thread)
//   2. fetch http://  (TCP connect + request + response + maybe decompress)
//   3. fetch https:// (adds TLS handshake)
//   4. DecompressionStream gzip round-trip (no network)
//
// Each probe has its own timeout so one hang doesn't block the rest.

const LOG = 'sdmc:/switch/net-debug.log';
let buf = '';
function log(s: string) {
	buf += s + '\n';
	console.log(s);
	try {
		Switch.writeFileSync(LOG, buf);
	} catch {}
}

function errStr(e: any): string {
	if (e && typeof e === 'object') {
		const head = `${e.name ?? 'Error'}: ${e.message ?? ''}`;
		const stack = typeof e.stack === 'string' ? e.stack : '';
		return stack && !stack.startsWith(e.name ?? 'Error')
			? `${head}\n${stack}`
			: stack || head;
	}
	return String(e);
}

function withTimeout<T>(label: string, p: Promise<T>, ms: number): Promise<T> {
	return new Promise<T>((resolve, reject) => {
		let done = false;
		const t = setTimeout(() => {
			if (done) return;
			done = true;
			reject(new Error(`${label}: timed out after ${ms}ms`));
		}, ms);
		p.then(
			(v) => {
				if (done) return;
				done = true;
				clearTimeout(t);
				resolve(v);
			},
			(e) => {
				if (done) return;
				done = true;
				clearTimeout(t);
				reject(e);
			},
		);
	});
}

async function probe(name: string, fn: () => Promise<void>, ms = 12000) {
	log(`\n>>> ${name}`);
	const start = Date.now();
	try {
		await withTimeout(name, fn(), ms);
		log(`<<< ${name} OK (${Date.now() - start}ms)`);
	} catch (e) {
		log(`!!! ${name} FAILED (${Date.now() - start}ms)`);
		log(errStr(e));
	}
}

async function main() {
	log(`=== net-debug started ${new Date().toISOString()} ===`);

	// 1. DNS — does getaddrinfo work on the worker thread?
	await probe('dns: Switch.resolveDns("example.com")', async () => {
		const ips = await (Switch as any).resolveDns('example.com');
		log(`    resolved: ${JSON.stringify(ips)}`);
	});

	// 2. http fetch — TCP connect + request + response (+ maybe decompress).
	await probe('fetch http://example.com', async () => {
		const res = await fetch('http://example.com');
		log(`    status=${res.status} ok=${res.ok}`);
		log(`    content-encoding=${res.headers.get('content-encoding')}`);
		log(`    content-length=${res.headers.get('content-length')}`);
		log(`    transfer-encoding=${res.headers.get('transfer-encoding')}`);
		const body = await res.text();
		log(`    body length=${body.length}, head=${JSON.stringify(body.slice(0, 80))}`);
	});

	// 3-pre. Manual HTTPS GET over a raw Socket — bypasses fetch's body
	// pipeline entirely to see (a) whether TLS READ delivers the response and
	// (b) the exact response framing (status line + headers).
	await probe('manual HTTPS GET letsencrypt.org (raw socket)', async () => {
		const sock = (Switch as any).connect('letsencrypt.org:443', {
			secureTransport: 'on',
			rejectUnauthorized: true,
		});
		await sock.opened;
		log('    opened (handshake done)');
		const w = sock.writable.getWriter();
		log('    writing request...');
		await w.write(
			new TextEncoder().encode(
				'GET / HTTP/1.1\r\nHost: letsencrypt.org\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n',
			),
		);
		log('    request written; reading...');
		w.releaseLock();
		const r = sock.readable.getReader();
		let total = 0;
		let head = '';
		const t0 = Date.now();
		while (Date.now() - t0 < 8000) {
			const { value, done } = await r.read();
			if (done) {
				log(`    read done (EOF) after ${total} bytes`);
				break;
			}
			if (head.length < 300) {
				head += new TextDecoder().decode(value).slice(0, 300 - head.length);
			}
			total += value.byteLength;
		}
		log(`    total bytes=${total}`);
		log(`    head=${JSON.stringify(head.slice(0, 200))}`);
		try {
			r.releaseLock();
		} catch {}
		sock.close();
	});

	// 3. https fetch (TLS handshake + verification + body). Uses a host whose
	// chain validates against the Switch's built-in CA set. Connection: close +
	// identity encoding to get a simple EOF-framed body.
	await probe('fetch https://letsencrypt.org', async () => {
		const res = await fetch('https://letsencrypt.org', {
			headers: { 'Accept-Encoding': 'identity' },
		} as any);
		log(`    status=${res.status} ok=${res.ok}`);
		log(`    content-length=${res.headers.get('content-length')}`);
		log(`    transfer-encoding=${res.headers.get('transfer-encoding')}`);
		log(`    content-encoding=${res.headers.get('content-encoding')}`);
		const body = await res.text();
		log(`    body length=${body.length}`);
	});

	// 3b. Verifying TLS across several hosts with different CA roots, to
	// confirm cert-chain validation works broadly.
	for (const host of [
		'letsencrypt.org', // ISRG Root X1
		'www.digicert.com', // DigiCert
		'github.com', // DigiCert / Sectigo
		'www.google.com', // Google Trust Services
	]) {
		await probe(`verify TLS ${host}:443`, async () => {
			const sock = (Switch as any).connect(`${host}:443`, {
				secureTransport: 'on',
				rejectUnauthorized: true,
			});
			await sock.opened;
			log('    verified OK');
			sock.close();
		});
	}

	// 4. Decompression round-trip (no network) — isolates the gzip path.
	await probe('DecompressionStream gzip round-trip (local)', async () => {
		const input = new TextEncoder().encode('hello '.repeat(1000));
		const cs = new CompressionStream('gzip');
		const w = cs.writable.getWriter();
		w.write(input);
		w.close();
		const gz = new Uint8Array(
			await new Response(cs.readable).arrayBuffer(),
		);
		log(`    compressed ${input.length} -> ${gz.length} bytes`);
		const ds = new DecompressionStream('gzip');
		const w2 = ds.writable.getWriter();
		w2.write(gz);
		w2.close();
		const out = new Uint8Array(
			await new Response(ds.readable).arrayBuffer(),
		);
		log(`    decompressed back to ${out.length} bytes`);
		if (out.length !== input.length) {
			throw new Error(`round-trip size mismatch: ${out.length} != ${input.length}`);
		}
	});

	log(`\n=== net-debug done ===`);
}

main();

console.log('net-debug running... press + to exit');
