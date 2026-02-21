// Audio Demo - nx.js
// Place .mp3, .wav, or .ogg files in romfs/ directory

const ctx = screen.getContext('2d');
const WIDTH = screen.width;
const HEIGHT = screen.height;

const files = ['romfs:/music.mp3', 'romfs:/sound.wav', 'romfs:/track.ogg'];
const labels = ['MP3', 'WAV', 'OGG'];
let selectedIndex = 0;
let audio: InstanceType<typeof Audio> | null = null;
let statusText = 'Idle';
let volumeLevel = 1.0;

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
		statusText = 'Error loading audio';
	});
}

addEventListener('gamepad', (e) => {
	const gp = navigator.getGamepads()[0];
	if (!gp) return;

	// D-Pad navigation
	if (gp.buttons[13]?.pressed) {
		// Down
		selectedIndex = Math.min(selectedIndex + 1, labels.length - 1);
	}
	if (gp.buttons[12]?.pressed) {
		// Up
		selectedIndex = Math.max(selectedIndex - 1, 0);
	}

	// A = play
	if (gp.buttons[0]?.pressed) {
		loadAndPlay(files[selectedIndex]);
	}

	// B = pause/resume
	if (gp.buttons[1]?.pressed && audio) {
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
	if (gp.buttons[2]?.pressed && audio) {
		audio.pause();
		audio.currentTime = 0;
		statusText = 'Stopped';
	}

	// L/R shoulder for volume
	if (gp.buttons[4]?.pressed) {
		volumeLevel = Math.max(0, volumeLevel - 0.1);
		if (audio) audio.volume = volumeLevel;
	}
	if (gp.buttons[5]?.pressed) {
		volumeLevel = Math.min(1, volumeLevel + 0.1);
		if (audio) audio.volume = volumeLevel;
	}
});

// Main loop
function frame() {
	drawUI();
	requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
