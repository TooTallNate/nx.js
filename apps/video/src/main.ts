// Video Demo - nx.js
// Place a video file (e.g. .webm / .mp4) at romfs/video.webm
//
// Controls:
//   A         = play / pause
//   Left / Right = seek -5s / +5s
//   Y         = toggle loop
//   Up / Down = volume

import { Button } from '@nx.js/constants';

const ctx = screen.getContext('2d');
const WIDTH = screen.width;
const HEIGHT = screen.height;

const video = new Video();
let statusText = 'Loading romfs:/video.webm ...';
let prevButtons: boolean[] = [];

video.loop = true;
video.addEventListener('canplay', () => {
	statusText = 'Ready — press A to play';
});
video.addEventListener('error', (e) => {
	statusText = `Error: ${e.error?.message ?? 'failed to load'} — place a video at romfs/video.webm`;
});
video.addEventListener('ended', () => {
	statusText = 'Ended — press A to replay';
});
video.src = 'romfs:/video.webm';

function buttonPressed(gp: Gamepad, index: number): boolean {
	const curr = gp.buttons[index]?.pressed ?? false;
	const prev = prevButtons[index] ?? false;
	return curr && !prev;
}

function handleInput() {
	const gp = navigator.getGamepads()[0];
	if (!gp) return;

	if (buttonPressed(gp, Button.A) && video.readyState >= Video.HAVE_METADATA) {
		if (video.paused) {
			video.play().then(() => {
				statusText = 'Playing';
			});
		} else {
			video.pause();
			statusText = 'Paused';
		}
	}
	if (buttonPressed(gp, Button.Left)) {
		video.currentTime = Math.max(0, video.currentTime - 5);
	}
	if (buttonPressed(gp, Button.Right)) {
		video.currentTime = Math.min(video.duration || 0, video.currentTime + 5);
	}
	if (buttonPressed(gp, Button.Y)) {
		video.loop = !video.loop;
	}
	if (buttonPressed(gp, Button.Up)) {
		video.volume = Math.min(1, +(video.volume + 0.1).toFixed(1));
	}
	if (buttonPressed(gp, Button.Down)) {
		video.volume = Math.max(0, +(video.volume - 0.1).toFixed(1));
	}

	prevButtons = gp.buttons.map((b) => b.pressed);
}

function frame() {
	handleInput();

	ctx.fillStyle = '#000';
	ctx.fillRect(0, 0, WIDTH, HEIGHT);

	// Letterbox the video into the screen.
	if (video.videoWidth > 0) {
		const scale = Math.min(
			WIDTH / video.videoWidth,
			(HEIGHT - 80) / video.videoHeight,
		);
		const w = video.videoWidth * scale;
		const h = video.videoHeight * scale;
		ctx.drawImage(video, (WIDTH - w) / 2, (HEIGHT - 80 - h) / 2, w, h);
	}

	// HUD
	ctx.fillStyle = '#16213e';
	ctx.fillRect(0, HEIGHT - 80, WIDTH, 80);
	const dur = video.duration;
	if (Number.isFinite(dur) && dur > 0) {
		ctx.fillStyle = '#0f3460';
		ctx.fillRect(40, HEIGHT - 64, WIDTH - 80, 8);
		ctx.fillStyle = '#e94560';
		ctx.fillRect(40, HEIGHT - 64, (WIDTH - 80) * (video.currentTime / dur), 8);
	}
	ctx.fillStyle = '#eee';
	ctx.font = '24px system-ui';
	ctx.textAlign = 'left';
	const time = `${video.currentTime.toFixed(1)} / ${Number.isFinite(dur) ? dur.toFixed(1) : '?'}s`;
	const q = video.getVideoPlaybackQuality();
	ctx.fillText(
		`${statusText}   ${time}   loop=${video.loop ? 'on' : 'off'}   vol=${Math.round(video.volume * 100)}%   frames=${q.totalVideoFrames} (${q.droppedVideoFrames} dropped)`,
		40,
		HEIGHT - 28,
	);

	requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
