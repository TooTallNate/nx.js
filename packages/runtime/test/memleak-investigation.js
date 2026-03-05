// Memory leak investigation: measure memory growth DURING streaming
// Run with: packages/runtime/test/build/nxjs-test packages/runtime/runtime.js packages/runtime/test/memleak-investigation.js

const ITERATIONS = 5000;
const CHUNK_SIZE = 1024; // 1KB to focus on Promise/object overhead not data

function mem(runGc = true) {
	if (runGc) __gc();
	return __memoryUsage();
}

function fmt(bytes) {
	if (Math.abs(bytes) > 1024 * 1024)
		return (bytes / 1024 / 1024).toFixed(1) + 'MB';
	if (Math.abs(bytes) > 1024) return (bytes / 1024).toFixed(1) + 'KB';
	return bytes + 'B';
}

async function runTest(label, setup, overrideN) {
	const n = overrideN || ITERATIONS;
	const samples = [];
	const sampleInterval = Math.floor(n / 5); // 5 samples

	await setup(n, (i) => {
		if (i % sampleInterval === 0 || i === n - 1) {
			// Don't run GC during sampling — too expensive. Just snapshot.
			const m = mem(false);
			samples.push({
				i,
				malloc: m.mallocSize,
				obj: m.objCount,
				array: m.arrayCount,
			});
		}
	});

	// Final measurement after completion + full GC
	const final = mem(true);
	samples.push({
		i: 'end',
		malloc: final.mallocSize,
		obj: final.objCount,
		array: final.arrayCount,
	});

	const first = samples[0];
	console.log(`\n[${label}]`);
	for (const s of samples) {
		const dm = s.malloc - first.malloc;
		const dobj = s.obj - first.obj;
		const darr = s.array - first.array;
		const pct = s.i === 'end' ? 'end' : `${((s.i / n) * 100).toFixed(0)}%`;
		console.log(
			`  ${pct.padStart(4)}: malloc ${dm >= 0 ? '+' : ''}${fmt(dm)}, obj ${dobj >= 0 ? '+' : ''}${dobj}, array ${darr >= 0 ? '+' : ''}${darr}`,
		);
	}
	const growth = samples[samples.length - 2].malloc - first.malloc;
	const perIter = growth / n;
	console.log(
		`  leak rate: ~${perIter.toFixed(1)} bytes/iter (${fmt(growth)} over ${ITERATIONS} iters)`,
	);
}

async function main() {
	console.log(
		`Memory leak investigation (${ITERATIONS} iterations, ${CHUNK_SIZE}B chunks)`,
	);

	// Test 1: Baseline - just awaiting promises
	await runTest('1. baseline promises', async (n, sample) => {
		for (let i = 0; i < n; i++) {
			await Promise.resolve(i);
			sample(i);
		}
	});

	// Test 2: ReadableStream read loop
	await runTest('2. ReadableStream read loop', async (n, sample) => {
		const data = new Uint8Array(CHUNK_SIZE);
		let count = 0;
		const rs = new ReadableStream({
			pull(c) {
				if (count >= n) {
					c.close();
				} else {
					c.enqueue(data);
					count++;
				}
			},
		});
		const reader = rs.getReader();
		let i = 0;
		while (true) {
			const { done } = await reader.read();
			if (done) break;
			sample(i++);
		}
	});

	// Test 3: WritableStream write loop
	await runTest('3. WritableStream write loop', async (n, sample) => {
		const data = new Uint8Array(CHUNK_SIZE);
		const ws = new WritableStream({ write() {} });
		const writer = ws.getWriter();
		for (let i = 0; i < n; i++) {
			await writer.write(data);
			sample(i);
		}
		await writer.close();
	});

	// Test 4: TransformStream writer/reader (no pipe)
	await runTest('4. TransformStream writer/reader', async (n, sample) => {
		const data = new Uint8Array(CHUNK_SIZE);
		const ts = new TransformStream({
			transform(c, ctrl) {
				ctrl.enqueue(c);
			},
		});
		const writer = ts.writable.getWriter();
		const reader = ts.readable.getReader();
		for (let i = 0; i < n; i++) {
			writer.write(data);
			await reader.read();
			sample(i);
		}
		await writer.close();
		await reader.read(); // drain close
	});

	// Test 5: pipeThrough (sync transform)
	await runTest('5. pipeThrough sync transform', async (n, sample) => {
		const data = new Uint8Array(CHUNK_SIZE);
		let count = 0;
		const rs = new ReadableStream({
			pull(c) {
				if (count >= n) {
					c.close();
				} else {
					c.enqueue(data);
					count++;
				}
			},
		});
		const ts = new TransformStream({
			transform(c, ctrl) {
				ctrl.enqueue(c);
			},
		});
		rs.pipeThrough(ts);
		const reader = ts.readable.getReader();
		let i = 0;
		while (true) {
			const { done } = await reader.read();
			if (done) break;
			sample(i++);
		}
	});

	// Test 6: pipeThrough (async transform)
	await runTest('6. pipeThrough async transform', async (n, sample) => {
		const data = new Uint8Array(CHUNK_SIZE);
		let count = 0;
		const rs = new ReadableStream({
			pull(c) {
				if (count >= n) {
					c.close();
				} else {
					c.enqueue(data);
					count++;
				}
			},
		});
		const ts = new TransformStream({
			async transform(c, ctrl) {
				await Promise.resolve();
				ctrl.enqueue(c);
			},
		});
		rs.pipeThrough(ts);
		const reader = ts.readable.getReader();
		let i = 0;
		while (true) {
			const { done } = await reader.read();
			if (done) break;
			sample(i++);
		}
	});

	// Test 7: pull() reads from another ReadableStream
	await runTest('7. nested reader.read() in pull()', async (n, sample) => {
		const data = new Uint8Array(CHUNK_SIZE);
		let countB = 0;
		const rsB = new ReadableStream({
			pull(c) {
				if (countB >= n) {
					c.close();
				} else {
					c.enqueue(data);
					countB++;
				}
			},
		});
		const readerB = rsB.getReader();
		let doneA = false;
		const rsA = new ReadableStream({
			async pull(c) {
				if (doneA) {
					c.close();
					return;
				}
				const r = await readerB.read();
				if (r.done) {
					c.close();
					doneA = true;
					return;
				}
				c.enqueue(r.value);
			},
		});
		const readerA = rsA.getReader();
		let i = 0;
		while (true) {
			const { done } = await readerA.read();
			if (done) break;
			sample(i++);
		}
	});

	// Test 8: pipeTo (manual - what pipeThrough calls internally)
	await runTest(
		'8. pipeTo (ReadableStream→WritableStream)',
		async (n, sample) => {
			const data = new Uint8Array(CHUNK_SIZE);
			let count = 0;
			const rs = new ReadableStream({
				pull(c) {
					if (count >= n) {
						c.close();
					} else {
						c.enqueue(data);
						count++;
					}
				},
			});
			let i = 0;
			const ws = new WritableStream({
				write() {
					sample(i++);
				},
			});
			await rs.pipeTo(ws);
		},
	);

	console.log('\nDone.');
	Switch.exit();
}

main().catch((e) => {
	console.log('ERROR:', e.message, e.stack);
	Switch.exit();
});
