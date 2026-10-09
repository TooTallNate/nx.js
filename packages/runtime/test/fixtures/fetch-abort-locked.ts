import { test } from '../src/tap';

test('cancel() on a locked ReadableStream throws TypeError', async (t) => {
	const source = new ReadableStream({
		start(controller) {
			controller.enqueue(new Uint8Array([1, 2, 3]));
		},
	});
	const reader = source.getReader();
	t.ok(source.locked, 'stream is locked after getReader');
	let threw = false;
	try {
		source.cancel();
	} catch (e: any) {
		threw = true;
		t.ok(e instanceof TypeError, 'cancel throws TypeError on locked stream');
	}
	t.ok(threw, 'cancel() threw');
	reader.releaseLock();
});

test('abort() on a locked WritableStream throws TypeError', async (t) => {
	const sink = new WritableStream();
	const writer = sink.getWriter();
	t.ok(sink.locked, 'stream is locked after getWriter');
	let threw = false;
	try {
		sink.abort();
	} catch (e: any) {
		threw = true;
		t.ok(e instanceof TypeError, 'abort throws TypeError on locked stream');
	}
	t.ok(threw, 'abort() threw');
	writer.releaseLock();
});

test('pipeThrough with abort signal aborts without TypeError', async (t) => {
	const ac = new AbortController();

	// Source that stays open (simulates a long-running HTTP response body)
	const source = new ReadableStream<Uint8Array>({
		start(controller) {
			controller.enqueue(new Uint8Array([1, 2, 3]));
			// intentionally not closed — keeps the pipe alive
		},
	});

	const transform = new TransformStream<Uint8Array, Uint8Array>();
	const piped = source.pipeThrough(transform, { signal: ac.signal });
	const reader = piped.getReader();

	// Read first chunk — this locks the piped stream
	const first = await reader.read();
	t.equal(first.done, false, 'first read not done');
	t.ok(first.value instanceof Uint8Array, 'got Uint8Array chunk');

	// Abort while stream is actively piped and locked
	ac.abort();

	// The next read should surface an AbortError (not a TypeError about locked
	// streams, which is the bug this fix addresses).
	try {
		const second = await reader.read();
		// Some implementations close the stream gracefully after abort
		t.ok(second.done, 'stream ended after abort');
	} catch (e: any) {
		t.notEqual(
			e.name,
			'TypeError',
			'error is not TypeError about locked streams',
		);
		t.ok(
			e instanceof DOMException || e instanceof Error,
			'abort error is DOMException or Error',
		);
	}
});

test('pipeThrough with already-aborted signal errors immediately', async (t) => {
	const ac = new AbortController();
	ac.abort();

	const source = new ReadableStream<Uint8Array>({
		start(controller) {
			controller.enqueue(new Uint8Array([1]));
		},
	});

	const transform = new TransformStream<Uint8Array, Uint8Array>();

	let threw = false;
	try {
		const piped = source.pipeThrough(transform, { signal: ac.signal });
		const reader = piped.getReader();
		await reader.read();
	} catch (e: any) {
		threw = true;
		t.notEqual(e.name, 'TypeError', 'error is not TypeError');
	}
	t.ok(threw, 'pipeThrough with aborted signal errored');
});
