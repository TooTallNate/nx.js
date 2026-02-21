// Audio Demo - nx.js
// Place .mp3, .wav, or .ogg files in the romfs/ directory

const ctx = screen.getContext('2d');
const WIDTH = screen.width;
const HEIGHT = screen.height;

const files = ['romfs:/music.mp3', 'romfs:/sound.wav', 'romfs:/track.ogg'];
const labels = ['MP3', 'WAV', 'OGG'];
let selectedIndex = 0;
let audio: InstanceType<typeof Audio> | null = null;
let statusText = 'Place audio files in romfs/';
let volumeLevel = 1.0;

// Track button states for edge detection (trigger on press, not hold)
let prevButtons: boolean[] = [];

function buttonPressed(gp: Gamepad, index: number): boolean {
	const curr = gp.buttons[index]?.pressed ?? false;
	const prev = prevButtons[index] ?? false;
	return curr && !prev;
}

function drawUI() {
	// Background
	ctx.fillStyle = '#1a1a2e';
	ctx.fillRect(0, 0, WIDTH, HEIGHT);

	// Title
	ctx.fillStyle = '#e94560';
	ctx.font = '48px system-ui';
	ctx.textAlign = 'center';
	ctx.fillText('Audio Demo', WIDTH / 2, 80);

	// File buttons
	ctx.font = '32px system-ui';
	for (let i = 0; i < labels.length; i++) {
		const y = 160 + i * 70;
		const isSelected = i === selectedIndex;

		ctx.fillStyle = isSelected ? '#e94560' : '#16213e';
		ctx.fillRect(WIDTH / 2 - 150, y - 30, 300, 55);

		ctx.fillStyle = '#eee';
		ctx.fillText(`Play ${labels[i]}`, WIDTH / 2, y + 8);
	}

	// Controls
	ctx.fillStyle = '#aaa';
	ctx.font = '24px system-ui';
	ctx.textAlign = 'left';
	const cx = 100;
	ctx.fillText('A = Play  |  B = Pause/Resume  |  X = Stop', cx, 420);
	ctx.fillText('D-Pad Up/Down = Select  |  L/R = Volume', cx, 460);

	// Status
	ctx.fillStyle = '#0f3460';
	ctx.fillRect(80, 500, WIDTH - 160, 120);

	ctx.fillStyle = '#eee';
	ctx.font = '28px system-ui';
	ctx.textAlign = 'left';
	ctx.fillText(`Status: ${statusText}`, 100, 540);

	if (audio) {
		const cur = audio.currentTime.toFixed(1);
		const dur = isNaN(audio.duration) ? '?' : audio.duration.toFixed(1);
		ctx.fillText(`Time: ${cur}s / ${dur}s`, 100, 575);
	}

	ctx.fillText(`Volume: ${Math.round(volumeLevel * 100)}%`, 100, 610);
}

function loadAndPlay(path: string) {
	if (audio) {
		audio.pause();
	}
	audio = new Audio(path);
	audio.volume = volumeLevel;
	statusText = 'Loading...';

	audio.addEventListener('canplaythrough', () => {
		statusText = 'Ready';
		audio!.play().then(() => {
			statusText = 'Playing';
		});
	});

	audio.addEventListener('ended', () => {
		statusText = 'Ended';
	});

	audio.addEventListener('error', () => {
		statusText = 'Error loading file';
	});
}

function handleInput() {
	const gp = navigator.getGamepads()[0];
	if (!gp) return;

	// D-Pad navigation
	if (buttonPressed(gp, 13)) {
		selectedIndex = Math.min(selectedIndex + 1, labels.length - 1);
	}
	if (buttonPressed(gp, 12)) {
		selectedIndex = Math.max(selectedIndex - 1, 0);
	}

	// A = play selected file
	if (buttonPressed(gp, 0)) {
		loadAndPlay(files[selectedIndex]);
	}

	// B = pause/resume
	if (buttonPressed(gp, 1) && audio) {
		if (audio.paused) {
			audio.play().then(() => {
				statusText = 'Playing';
			});
		} else {
			audio.pause();
			statusText = 'Paused';
		}
	}

	// X = stop
	if (buttonPressed(gp, 2) && audio) {
		audio.pause();
		audio.currentTime = 0;
		statusText = 'Stopped';
	}

	// L shoulder = volume down
	if (buttonPressed(gp, 4)) {
		volumeLevel = Math.max(0, +(volumeLevel - 0.1).toFixed(1));
		if (audio) audio.volume = volumeLevel;
	}
	// R shoulder = volume up
	if (buttonPressed(gp, 5)) {
		volumeLevel = Math.min(1, +(volumeLevel + 0.1).toFixed(1));
		if (audio) audio.volume = volumeLevel;
	}

	// Save button states for next frame
	prevButtons = gp.buttons.map((b) => b.pressed);
}

// Main loop
function frame() {
	handleInput();
	drawUI();
	requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
