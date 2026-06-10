import { test } from '../src/tap';

// Web Audio API conformance tests. All rendering goes through
// OfflineAudioContext so the output is deterministic and does not depend on
// Chrome's autoplay policy (a real-time AudioContext would start "suspended"
// in headless Chrome without a user gesture).

const RATE = 48000;

function throwsName(
	t: any,
	fn: () => unknown,
	name: string,
	msg: string,
): void {
	try {
		fn();
		t.fail(msg);
	} catch (err: any) {
		t.equal(err?.name, name, msg);
	}
}

function closeTo(a: number, b: number, eps = 1e-4): boolean {
	return Math.abs(a - b) <= eps;
}

function maxDiff(data: Float32Array, expected: (i: number) => number): number {
	let max = 0;
	for (let i = 0; i < data.length; i++) {
		const d = Math.abs(data[i] - expected(i));
		if (d > max) max = d;
	}
	return max;
}

// --- AudioBuffer ---

test('AudioBuffer basics', (t) => {
	const buf = new AudioBuffer({
		numberOfChannels: 2,
		length: 1000,
		sampleRate: RATE,
	});
	t.equal(buf.numberOfChannels, 2, 'numberOfChannels is 2');
	t.equal(buf.length, 1000, 'length is 1000');
	t.equal(buf.sampleRate, RATE, 'sampleRate is 48000');
	t.ok(closeTo(buf.duration, 1000 / RATE, 1e-9), 'duration is length/rate');

	const ch0 = buf.getChannelData(0);
	t.ok(ch0 instanceof Float32Array, 'getChannelData returns Float32Array');
	t.equal(ch0.length, 1000, 'channel data length matches');
	t.equal(ch0[0], 0, 'channel data is zero-initialized');
	t.equal(buf.getChannelData(0), ch0, 'getChannelData returns same array');

	ch0[0] = 0.5;
	const dest = new Float32Array(4);
	buf.copyFromChannel(dest, 0);
	t.equal(dest[0], 0.5, 'copyFromChannel copies data');

	const src = new Float32Array([1, 2, 3, 4]);
	buf.copyToChannel(src, 1, 2);
	t.equal(buf.getChannelData(1)[2], 1, 'copyToChannel honors bufferOffset');
	t.equal(buf.getChannelData(1)[5], 4, 'copyToChannel copies all values');

	throwsName(
		t,
		() => buf.getChannelData(2),
		'IndexSizeError',
		'getChannelData throws for invalid channel',
	);
	throwsName(
		t,
		() =>
			new AudioBuffer({ numberOfChannels: 0, length: 10, sampleRate: RATE }),
		'NotSupportedError',
		'constructor throws for 0 channels',
	);
	throwsName(
		t,
		() =>
			new AudioBuffer({ numberOfChannels: 1, length: 0, sampleRate: RATE }),
		'NotSupportedError',
		'constructor throws for 0 length',
	);
	throwsName(
		t,
		() =>
			new AudioBuffer({ numberOfChannels: 1, length: 10, sampleRate: 1 }),
		'NotSupportedError',
		'constructor throws for bad sampleRate',
	);
});

// --- OfflineAudioContext basics ---

test('OfflineAudioContext basics', (t) => {
	const ctx = new OfflineAudioContext(1, 1280, RATE);
	t.equal(ctx.length, 1280, 'length is 1280');
	t.equal(ctx.sampleRate, RATE, 'sampleRate is 48000');
	t.equal(ctx.state, 'suspended', 'initial state is suspended');
	t.equal(ctx.currentTime, 0, 'initial currentTime is 0');
	t.ok(ctx.destination instanceof AudioDestinationNode, 'destination type');
	t.equal(ctx.destination.numberOfInputs, 1, 'destination numberOfInputs');
	t.equal(ctx.destination.numberOfOutputs, 0, 'destination numberOfOutputs');
	t.equal(ctx.destination.context, ctx, 'destination context is ctx');
});

// --- Source playback through gain ---

test('buffer source through gain', async (t) => {
	const N = 1280;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	const data = buffer.getChannelData(0);
	for (let i = 0; i < N; i++) data[i] = 1;

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	t.equal(gain.gain.value, 1, 'gain defaults to 1');
	gain.gain.value = 0.25;
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const rendered = await ctx.startRendering();
	t.equal(rendered.length, N, 'rendered buffer length');
	t.equal(rendered.numberOfChannels, 1, 'rendered buffer channels');
	t.equal(rendered.sampleRate, RATE, 'rendered buffer sampleRate');
	const out = rendered.getChannelData(0);
	t.ok(maxDiff(out, () => 0.25) < 1e-6, 'all samples scaled by gain');
	t.equal(ctx.state, 'closed', 'state is closed after rendering');
});

// --- Sample-accurate playback (no processing) ---

test('buffer source passthrough is sample-accurate', async (t) => {
	const N = 512;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	const data = buffer.getChannelData(0);
	for (let i = 0; i < N; i++) data[i] = Math.sin(i / 10);

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	source.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(
		maxDiff(out, (i) => Math.sin(i / 10)) < 1e-6,
		'output matches buffer exactly',
	);
});

// --- start() offset and duration ---

test('buffer source start offset and duration', async (t) => {
	const N = 1024;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N * 2, RATE);
	const data = buffer.getChannelData(0);
	for (let i = 0; i < data.length; i++) data[i] = i / data.length;

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	source.connect(ctx.destination);
	// Start 256 frames into the buffer, play 128 frames worth.
	source.start(0, 256 / RATE, 128 / RATE);

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(closeTo(out[0], data[256], 1e-6), 'first sample honors offset');
	t.ok(closeTo(out[127], data[256 + 127], 1e-6), 'last sample before cutoff');
	t.equal(out[200], 0, 'silent after duration elapses');
	t.equal(out[N - 1], 0, 'silent at end');
});

// --- Delayed start ---

test('buffer source delayed start', async (t) => {
	const N = 512;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	source.connect(ctx.destination);
	source.start(256 / RATE);

	const out = (await ctx.startRendering()).getChannelData(0);
	t.equal(out[0], 0, 'silent before start time');
	t.equal(out[255], 0, 'silent right before start time');
	t.equal(out[256], 1, 'plays at start time');
	t.equal(out[N - 1], 1, 'still playing at end');
});

// --- Looping ---

test('buffer source looping', async (t) => {
	const N = 1000;
	const period = 100;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, period, RATE);
	const data = buffer.getChannelData(0);
	for (let i = 0; i < period; i++) data[i] = (i + 1) / period;

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	source.loop = true;
	t.equal(source.loop, true, 'loop property set');
	source.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(
		closeTo(out[0], data[0], 1e-6) &&
			closeTo(out[period], data[0], 1e-6) &&
			closeTo(out[5 * period + 50], data[50], 1e-6),
		'looped content repeats',
	);
	t.ok(closeTo(out[N - 1], data[(N - 1) % period], 1e-6), 'loops to the end');
});

// --- playbackRate ---

test('buffer source playbackRate', async (t) => {
	const N = 256;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N * 2, RATE);
	const data = buffer.getChannelData(0);
	for (let i = 0; i < data.length; i++) data[i] = i;

	const source = ctx.createBufferSource();
	t.equal(source.playbackRate.value, 1, 'playbackRate defaults to 1');
	t.equal(source.detune.value, 0, 'detune defaults to 0');
	source.buffer = buffer;
	source.playbackRate.value = 2;
	source.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(
		maxDiff(out, (i) => 2 * i) < 1e-2,
		'output advances at twice the rate',
	);
});

// --- Gain automation: setValueAtTime + linearRamp ---

test('gain linear ramp automation', async (t) => {
	const N = 1280;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	gain.gain.setValueAtTime(0, 0);
	gain.gain.linearRampToValueAtTime(1, N / RATE);
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(
		maxDiff(out, (i) => i / N) < 1e-3,
		'gain ramps linearly from 0 to 1',
	);
});

// --- Gain automation: setValueAtTime steps ---

test('gain setValueAtTime steps', async (t) => {
	const N = 1024;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	gain.gain.setValueAtTime(0.25, 0);
	gain.gain.setValueAtTime(0.75, 512 / RATE);
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(closeTo(out[0], 0.25, 1e-6), 'first step value');
	t.ok(closeTo(out[511], 0.25, 1e-6), 'holds until second step');
	t.ok(closeTo(out[512], 0.75, 1e-6), 'second step value');
	t.ok(closeTo(out[N - 1], 0.75, 1e-6), 'holds to the end');
});

// --- Exponential ramp ---

test('gain exponential ramp automation', async (t) => {
	const N = 1280;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	gain.gain.setValueAtTime(0.01, 0);
	gain.gain.exponentialRampToValueAtTime(1, N / RATE);
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(
		maxDiff(out, (i) => 0.01 * Math.pow(100, i / N)) < 1e-3,
		'gain ramps exponentially',
	);
});

// --- setTargetAtTime ---

test('gain setTargetAtTime automation', async (t) => {
	const N = 1280;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const tc = 0.005;
	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	gain.gain.setValueAtTime(1, 0);
	gain.gain.setTargetAtTime(0, 0, tc);
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(
		maxDiff(out, (i) => Math.exp(-(i / RATE) / tc)) < 1e-3,
		'gain decays toward target',
	);
});

// --- setValueCurveAtTime ---

test('gain setValueCurveAtTime automation', async (t) => {
	const N = 1024;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const curve = new Float32Array([0, 1, 0.5]);
	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	gain.gain.setValueCurveAtTime(curve, 0, 512 / RATE);
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(closeTo(out[0], 0, 1e-5), 'curve start value');
	t.ok(closeTo(out[128], 0.5, 2e-3), 'curve interpolates first segment');
	t.ok(closeTo(out[256], 1, 2e-3), 'curve midpoint value');
	t.ok(closeTo(out[384], 0.75, 2e-3), 'curve interpolates second segment');
	t.ok(closeTo(out[600], 0.5, 1e-5), 'holds final curve value');
});

// --- cancelScheduledValues ---

test('cancelScheduledValues removes future events', async (t) => {
	const N = 1024;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, N, RATE);
	buffer.getChannelData(0).fill(1);

	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const gain = ctx.createGain();
	gain.gain.setValueAtTime(0.5, 0);
	gain.gain.setValueAtTime(0.9, 512 / RATE);
	gain.gain.cancelScheduledValues(256 / RATE);
	source.connect(gain);
	gain.connect(ctx.destination);
	source.start();

	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(closeTo(out[0], 0.5, 1e-6), 'kept event applies');
	t.ok(closeTo(out[N - 1], 0.5, 1e-6), 'canceled event does not apply');
});

// --- StereoPannerNode ---

test('stereo panner mono input', async (t) => {
	const N = 384;
	const SQRT1_2 = Math.SQRT1_2;
	for (const [pan, gl, gr, name] of [
		[0, SQRT1_2, SQRT1_2, 'center'],
		[-1, 1, 0, 'left'],
		[1, 0, 1, 'right'],
	] as [number, number, number, string][]) {
		const ctx = new OfflineAudioContext(2, N, RATE);
		const buffer = ctx.createBuffer(1, N, RATE);
		buffer.getChannelData(0).fill(1);
		const source = ctx.createBufferSource();
		source.buffer = buffer;
		const panner = ctx.createStereoPanner();
		panner.pan.value = pan;
		source.connect(panner);
		panner.connect(ctx.destination);
		source.start();
		const rendered = await ctx.startRendering();
		const l = rendered.getChannelData(0);
		const r = rendered.getChannelData(1);
		t.ok(closeTo(l[100], gl, 1e-5), `pan ${name}: left gain`);
		t.ok(closeTo(r[100], gr, 1e-5), `pan ${name}: right gain`);
	}
});

test('stereo panner stereo input', async (t) => {
	const N = 384;
	const ctx = new OfflineAudioContext(2, N, RATE);
	const buffer = ctx.createBuffer(2, N, RATE);
	buffer.getChannelData(0).fill(0.5);
	buffer.getChannelData(1).fill(0.25);
	const source = ctx.createBufferSource();
	source.buffer = buffer;
	const panner = ctx.createStereoPanner();
	t.equal(panner.pan.value, 0, 'pan defaults to 0');
	source.connect(panner);
	panner.connect(ctx.destination);
	source.start();
	const rendered = await ctx.startRendering();
	t.ok(
		closeTo(rendered.getChannelData(0)[100], 0.5, 1e-5),
		'pan 0 stereo passthrough left',
	);
	t.ok(
		closeTo(rendered.getChannelData(1)[100], 0.25, 1e-5),
		'pan 0 stereo passthrough right',
	);
});

// --- Stereo downmix to mono destination ---

test('stereo source downmix to mono destination', async (t) => {
	const N = 384;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(2, N, RATE);
	buffer.getChannelData(0).fill(0.8);
	buffer.getChannelData(1).fill(0.4);
	const source = ctx.createBufferSource();
	source.buffer = buffer;
	source.connect(ctx.destination);
	source.start();
	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(closeTo(out[100], 0.6, 1e-5), 'mono downmix is 0.5*(L+R)');
});

// --- Fan-in summing ---

test('multiple sources sum at destination', async (t) => {
	const N = 384;
	const ctx = new OfflineAudioContext(1, N, RATE);
	for (const value of [0.25, 0.5]) {
		const buffer = ctx.createBuffer(1, N, RATE);
		buffer.getChannelData(0).fill(value);
		const source = ctx.createBufferSource();
		source.buffer = buffer;
		source.connect(ctx.destination);
		source.start();
	}
	const out = (await ctx.startRendering()).getChannelData(0);
	t.ok(closeTo(out[100], 0.75, 1e-5), 'inputs are summed');
});

// --- ended event ---

test('source ended event fires', async (t) => {
	const N = 2048;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const buffer = ctx.createBuffer(1, 256, RATE);
	buffer.getChannelData(0).fill(1);
	const source = ctx.createBufferSource();
	source.buffer = buffer;
	source.connect(ctx.destination);
	const ended = new Promise<boolean>((resolve) => {
		const timo = setTimeout(() => resolve(false), 3000);
		source.onended = () => {
			clearTimeout(timo);
			resolve(true);
		};
	});
	source.start();
	await ctx.startRendering();
	t.ok(await ended, 'ended event fired');
});

// --- oncomplete event ---

test('OfflineAudioContext complete event', async (t) => {
	const N = 384;
	const ctx = new OfflineAudioContext(1, N, RATE);
	const complete = new Promise<any>((resolve) => {
		const timo = setTimeout(() => resolve(null), 3000);
		ctx.oncomplete = (ev) => {
			clearTimeout(timo);
			resolve(ev);
		};
	});
	const rendered = await ctx.startRendering();
	const ev = await complete;
	t.ok(ev !== null, 'complete event fired');
	t.equal(ev?.renderedBuffer?.length, N, 'event has renderedBuffer');
	t.equal(rendered.length, N, 'startRendering resolves with buffer');
});

// --- decodeAudioData (WAV built in JS) ---

function buildWav(samples: Float32Array, sampleRate: number): ArrayBuffer {
	const dataSize = samples.length * 2;
	const ab = new ArrayBuffer(44 + dataSize);
	const view = new DataView(ab);
	const writeStr = (off: number, s: string) => {
		for (let i = 0; i < s.length; i++) view.setUint8(off + i, s.charCodeAt(i));
	};
	writeStr(0, 'RIFF');
	view.setUint32(4, 36 + dataSize, true);
	writeStr(8, 'WAVE');
	writeStr(12, 'fmt ');
	view.setUint32(16, 16, true); // fmt chunk size
	view.setUint16(20, 1, true); // PCM
	view.setUint16(22, 1, true); // mono
	view.setUint32(24, sampleRate, true);
	view.setUint32(28, sampleRate * 2, true); // byte rate
	view.setUint16(32, 2, true); // block align
	view.setUint16(34, 16, true); // bits per sample
	writeStr(36, 'data');
	view.setUint32(40, dataSize, true);
	for (let i = 0; i < samples.length; i++) {
		const s = Math.max(-1, Math.min(1, samples[i]));
		view.setInt16(44 + i * 2, Math.round(s * 32767), true);
	}
	return ab;
}

test('decodeAudioData decodes WAV', async (t) => {
	const N = 4800;
	const samples = new Float32Array(N);
	for (let i = 0; i < N; i++) samples[i] = Math.sin((i / RATE) * 2 * Math.PI * 440) * 0.5;
	const wav = buildWav(samples, RATE);

	const ctx = new OfflineAudioContext(1, 384, RATE);
	const buffer = await ctx.decodeAudioData(wav);
	t.equal(buffer.numberOfChannels, 1, 'decoded channel count');
	t.equal(buffer.sampleRate, RATE, 'decoded sample rate');
	t.equal(buffer.length, N, 'decoded length');
	const data = buffer.getChannelData(0);
	t.ok(
		maxDiff(data, (i) => samples[i]) < 1e-3,
		'decoded samples match (within s16 quantization)',
	);
	t.equal(wav.byteLength, 0, 'input ArrayBuffer is detached');
});

test('decodeAudioData rejects garbage', async (t) => {
	const ctx = new OfflineAudioContext(1, 384, RATE);
	const garbage = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]).buffer;
	try {
		await ctx.decodeAudioData(garbage);
		t.fail('should have rejected');
	} catch (err: any) {
		t.equal(err.name, 'EncodingError', 'rejects with EncodingError');
	}
});

// --- API errors ---

test('Web Audio API errors', async (t) => {
	const ctx = new OfflineAudioContext(1, 384, RATE);
	const source = ctx.createBufferSource();
	const buffer = ctx.createBuffer(1, 100, RATE);

	throwsName(t, () => source.stop(), 'InvalidStateError', 'stop before start throws');

	source.buffer = buffer;
	throwsName(
		t,
		() => {
			source.buffer = buffer;
		},
		'InvalidStateError',
		'setting buffer twice throws',
	);
	source.buffer = null;
	t.ok(source.buffer === null, 'buffer can be cleared');

	source.start();
	throwsName(t, () => source.start(), 'InvalidStateError', 'start twice throws');

	const gain = ctx.createGain();
	throwsName(
		t,
		() => gain.gain.setValueAtTime(1, -1),
		'RangeError',
		'negative time throws',
	);
	throwsName(
		t,
		() => gain.gain.exponentialRampToValueAtTime(0, 1),
		'RangeError',
		'exponential ramp to 0 throws',
	);
	throwsName(
		t,
		() => gain.gain.setTargetAtTime(1, 0, -1),
		'RangeError',
		'negative timeConstant throws',
	);
	throwsName(
		t,
		() => gain.gain.setValueCurveAtTime(new Float32Array([1]), 0, 0.1),
		'InvalidStateError',
		'short curve throws',
	);

	const ctx2 = new OfflineAudioContext(1, 384, RATE);
	const gain2 = ctx2.createGain();
	throwsName(
		t,
		() => gain.connect(gain2),
		'InvalidAccessError',
		'cross-context connect throws',
	);

	throwsName(
		t,
		() => new OfflineAudioContext(0, 384, RATE),
		'NotSupportedError',
		'OfflineAudioContext with 0 channels throws',
	);
});

// --- Node properties ---

test('AudioNode properties', (t) => {
	const ctx = new OfflineAudioContext(1, 384, RATE);
	const source = ctx.createBufferSource();
	t.equal(source.numberOfInputs, 0, 'source numberOfInputs');
	t.equal(source.numberOfOutputs, 1, 'source numberOfOutputs');
	const gain = ctx.createGain();
	t.equal(gain.numberOfInputs, 1, 'gain numberOfInputs');
	t.equal(gain.numberOfOutputs, 1, 'gain numberOfOutputs');
	t.equal(gain.context, ctx, 'gain context');
	t.equal(gain.connect(ctx.destination), ctx.destination, 'connect returns destination');
	gain.disconnect();
	t.ok(source instanceof AudioScheduledSourceNode, 'source instanceof AudioScheduledSourceNode');
	t.ok(source instanceof AudioNode, 'source instanceof AudioNode');
	t.ok(gain instanceof AudioNode, 'gain instanceof AudioNode');
	t.ok(gain.gain instanceof AudioParam, 'gain.gain instanceof AudioParam');
	t.equal(gain.gain.defaultValue, 1, 'gain defaultValue');
	const panner = ctx.createStereoPanner();
	t.equal(panner.pan.minValue, -1, 'pan minValue');
	t.equal(panner.pan.maxValue, 1, 'pan maxValue');
	t.ok(ctx instanceof BaseAudioContext, 'ctx instanceof BaseAudioContext');
});
