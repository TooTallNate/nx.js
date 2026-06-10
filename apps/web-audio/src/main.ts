// Web Audio API Demo - nx.js
// All sounds are synthesized procedurally into AudioBuffers — no asset files.

import { Button } from '@nx.js/constants';

const ctx2d = screen.getContext('2d');
const WIDTH = screen.width;
const HEIGHT = screen.height;

const audio = new AudioContext();

// Master gain so the volume controls affect every sound.
const master = audio.createGain();
master.connect(audio.destination);

let statusText = 'Press a button to play a sound';
let volumeLevel = 1.0;
let musicSource: AudioBufferSourceNode | null = null;

// --- Procedural sound generation ------------------------------------------

/** Create a mono AudioBuffer filled by `fn(t)` (t in seconds). */
function generate(duration: number, fn: (t: number) => number): AudioBuffer {
	const length = Math.floor(duration * audio.sampleRate);
	const buffer = audio.createBuffer(1, length, audio.sampleRate);
	const data = buffer.getChannelData(0);
	for (let i = 0; i < length; i++) {
		data[i] = fn(i / audio.sampleRate);
	}
	return buffer;
}

const square = (phase: number) => (Math.sin(phase) >= 0 ? 1 : -1);

// "Coin" — square wave arpeggio (B5 → E6) with a quick decay.
const coinBuffer = generate(0.45, (t) => {
	const freq = t < 0.08 ? 987.77 : 1318.51;
	const env = Math.exp(-t * 9);
	return square(2 * Math.PI * freq * t) * env * 0.25;
});

// "Laser" — descending frequency sweep.
const laserBuffer = generate(0.3, (t) => {
	const freq = 1800 - 4500 * t;
	const env = Math.exp(-t * 12);
	return Math.sin(2 * Math.PI * freq * t) * env * 0.35;
});

// "Explosion" — white noise with a long decay.
const explosionBuffer = generate(0.8, (t) => {
	const env = Math.exp(-t * 5);
	return (Math.random() * 2 - 1) * env * 0.5;
});

// "Blip" — short sine ping, used for the pan demo.
const blipBuffer = generate(0.2, (t) => {
	const env = Math.exp(-t * 20);
	return Math.sin(2 * Math.PI * 880 * t) * env * 0.4;
});

// "Music" — a little looped bass arpeggio.
const musicBuffer = generate(2.0, (t) => {
	const notes = [110, 138.59, 164.81, 138.59]; // A2 C#3 E3 C#3
	const step = Math.floor(t * 4) % notes.length;
	const local = (t * 4) % 1;
	const env = Math.min(1, local * 30) * Math.exp(-local * 4);
	return (
		(Math.sin(2 * Math.PI * notes[step] * t) +
			0.5 * square(2 * Math.PI * notes[step] * 2 * t)) *
		env *
		0.2
	);
});

// --- Playback helpers ------------------------------------------------------

function playBuffer(buffer: AudioBuffer, pan = 0): AudioBufferSourceNode {
	const source = audio.createBufferSource();
	source.buffer = buffer;
	if (pan !== 0) {
		const panner = audio.createStereoPanner();
		panner.pan.value = pan;
		source.connect(panner);
		panner.connect(master);
	} else {
		source.connect(master);
	}
	source.start();
	return source;
}

// "Chord" — three detuned sources with a gain envelope driven by
// AudioParam automation (setValueAtTime + ramps).
function playChord() {
	const now = audio.currentTime;
	const env = audio.createGain();
	env.gain.setValueAtTime(0, now);
	env.gain.linearRampToValueAtTime(0.2, now + 0.05);
	env.gain.exponentialRampToValueAtTime(0.001, now + 1.5);
	env.connect(master);
	for (const freq of [220, 277.18, 329.63]) {
		const buffer = generate(1.5, (t) => Math.sin(2 * Math.PI * freq * t));
		const source = audio.createBufferSource();
		source.buffer = buffer;
		source.connect(env);
		source.start();
	}
}

function toggleMusic() {
	if (musicSource) {
		musicSource.stop();
		musicSource = null;
		statusText = 'Music stopped';
	} else {
		musicSource = audio.createBufferSource();
		musicSource.buffer = musicBuffer;
		musicSource.loop = true;
		musicSource.connect(master);
		musicSource.start();
		statusText = 'Music looping (Minus to stop)';
	}
}

// --- UI / input ------------------------------------------------------------

let prevButtons: boolean[] = [];

function buttonPressed(gp: Gamepad, index: number): boolean {
	const curr = gp.buttons[index]?.pressed ?? false;
	const prev = prevButtons[index] ?? false;
	return curr && !prev;
}

function handleInput() {
	const gp = navigator.getGamepads()[0];
	if (!gp) return;

	if (buttonPressed(gp, Button.A)) {
		playBuffer(coinBuffer);
		statusText = 'Coin!';
	}
	if (buttonPressed(gp, Button.B)) {
		playBuffer(laserBuffer);
		statusText = 'Laser!';
	}
	if (buttonPressed(gp, Button.X)) {
		playBuffer(explosionBuffer);
		statusText = 'Explosion!';
	}
	if (buttonPressed(gp, Button.Y)) {
		playChord();
		statusText = 'Chord (gain automation)';
	}
	if (buttonPressed(gp, Button.L)) {
		playBuffer(blipBuffer, -1);
		statusText = 'Blip (left)';
	}
	if (buttonPressed(gp, Button.R)) {
		playBuffer(blipBuffer, 1);
		statusText = 'Blip (right)';
	}
	if (buttonPressed(gp, Button.Minus)) {
		toggleMusic();
	}
	if (buttonPressed(gp, Button.Down)) {
		volumeLevel = Math.max(0, +(volumeLevel - 0.1).toFixed(1));
		master.gain.value = volumeLevel;
	}
	if (buttonPressed(gp, Button.Up)) {
		volumeLevel = Math.min(1, +(volumeLevel + 0.1).toFixed(1));
		master.gain.value = volumeLevel;
	}

	prevButtons = gp.buttons.map((b) => b.pressed);
}

function drawUI() {
	ctx2d.fillStyle = '#1a1a2e';
	ctx2d.fillRect(0, 0, WIDTH, HEIGHT);

	ctx2d.fillStyle = '#e94560';
	ctx2d.font = '48px system-ui';
	ctx2d.textAlign = 'center';
	ctx2d.fillText('Web Audio API Demo', WIDTH / 2, 90);

	ctx2d.fillStyle = '#eee';
	ctx2d.font = '28px system-ui';
	ctx2d.textAlign = 'left';
	const cx = 160;
	const lines = [
		'A = Coin       B = Laser',
		'X = Explosion  Y = Chord (gain automation)',
		'L = Blip left  R = Blip right (StereoPanner)',
		'Minus = Toggle looped music',
		'D-Pad Up/Down = Master volume',
	];
	lines.forEach((line, i) => {
		ctx2d.fillText(line, cx, 180 + i * 50);
	});

	ctx2d.fillStyle = '#0f3460';
	ctx2d.fillRect(80, 480, WIDTH - 160, 140);
	ctx2d.fillStyle = '#eee';
	ctx2d.fillText(`Status: ${statusText}`, 100, 530);
	ctx2d.fillText(
		`Volume: ${Math.round(volumeLevel * 100)}%   currentTime: ${audio.currentTime.toFixed(1)}s`,
		100,
		580,
	);
}

function frame() {
	handleInput();
	drawUI();
	requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
