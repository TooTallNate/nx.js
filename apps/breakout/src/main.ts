import { FPS } from './fps';
import { Button } from '@nx.js/constants';

// ── Constants ────────────────────────────────────────────────────────
const W = screen.width; // 1280
const H = screen.height; // 720
const ctx = screen.getContext('2d');

// Brick grid
const brickRowCount = 5;
const brickColumnCount = 8;
const brickWidth = 120;
const brickHeight = 30;
const brickPadding = 12;
const brickOffsetTop = 80;
const brickOffsetLeft = (W - (brickColumnCount * (brickWidth + brickPadding) - brickPadding)) / 2;

const rowColors = ['#e74c3c', '#e67e22', '#f1c40f', '#2ecc71', '#3498db'];

// Paddle
const paddleHeight = 18;
const paddleWidth = 160;
const paddleRadius = paddleHeight / 2;
const paddleY = H - 50;
const paddleSpeed = 10;

// Ball
const ballRadius = 10;
const initialBallSpeed = 6;

// ── State ────────────────────────────────────────────────────────────
enum State {
	Playing,
	Paused,
	GameOver,
	Won,
}

interface Brick {
	x: number;
	y: number;
	alive: boolean;
}

let state: State;
let paddleX: number;
let ballX: number;
let ballY: number;
let ballDX: number;
let ballDY: number;
let score: number;
let lives: number;
let bricks: Brick[][];
let touchX: number | null = null;

function initBricks(): Brick[][] {
	const b: Brick[][] = [];
	for (let c = 0; c < brickColumnCount; c++) {
		b[c] = [];
		for (let r = 0; r < brickRowCount; r++) {
			const x = brickOffsetLeft + c * (brickWidth + brickPadding);
			const y = brickOffsetTop + r * (brickHeight + brickPadding);
			b[c][r] = { x, y, alive: true };
		}
	}
	return b;
}

function resetBall() {
	ballX = W / 2;
	ballY = paddleY - ballRadius - 2;
	// Random-ish angle
	const angle = -Math.PI / 4 + Math.random() * (-Math.PI / 2);
	ballDX = initialBallSpeed * Math.cos(angle);
	ballDY = initialBallSpeed * Math.sin(angle);
	if (ballDY > 0) ballDY = -ballDY; // always start going up
}

function start() {
	score = 0;
	lives = 3;
	paddleX = (W - paddleWidth) / 2;
	bricks = initBricks();
	resetBall();
	state = State.Playing;
}

// ── Drawing ──────────────────────────────────────────────────────────
function drawBackground() {
	ctx.fillStyle = '#1a1a2e';
	ctx.fillRect(0, 0, W, H);
}

function drawBricks() {
	for (let c = 0; c < brickColumnCount; c++) {
		for (let r = 0; r < brickRowCount; r++) {
			const brick = bricks[c][r];
			if (!brick.alive) continue;
			ctx.fillStyle = rowColors[r];
			ctx.beginPath();
			ctx.roundRect(brick.x, brick.y, brickWidth, brickHeight, 4);
			ctx.fill();
		}
	}
}

function drawBall() {
	const gradient = ctx.createRadialGradient(
		ballX - 3, ballY - 3, 1,
		ballX, ballY, ballRadius,
	);
	gradient.addColorStop(0, '#ffffff');
	gradient.addColorStop(1, '#aaaaff');
	ctx.beginPath();
	ctx.arc(ballX, ballY, ballRadius, 0, Math.PI * 2);
	ctx.fillStyle = gradient;
	ctx.fill();
}

function drawPaddle() {
	ctx.fillStyle = '#ecf0f1';
	ctx.beginPath();
	ctx.roundRect(paddleX, paddleY, paddleWidth, paddleHeight, paddleRadius);
	ctx.fill();
}

function drawHUD() {
	ctx.fillStyle = '#ecf0f1';
	ctx.font = '22px system-ui';
	ctx.fillText(`Score: ${score}`, 32, 32);
	ctx.fillText(`Lives: ${lives}`, 200, 32);
}

function drawOverlay(title: string, subtitle: string, color: string) {
	const boxW = 500;
	const boxH = 240;
	const boxX = W / 2 - boxW / 2;
	const boxY = H / 2 - boxH / 2;
	ctx.fillStyle = 'rgba(0, 0, 0, 0.6)';
	ctx.fillRect(boxX, boxY, boxW, boxH);
	ctx.fillStyle = color;
	ctx.font = '48px system-ui';
	ctx.fillText(title, boxX + 50, boxY + 80);
	ctx.fillStyle = '#ecf0f1';
	ctx.font = '28px system-ui';
	ctx.fillText(`Score: ${score}`, boxX + 50, boxY + 130);
	ctx.font = '22px system-ui';
	ctx.fillText(subtitle, boxX + 50, boxY + 180);
	ctx.fillText('Press + to exit', boxX + 50, boxY + 215);
}

// ── Game logic ───────────────────────────────────────────────────────
function collisionDetection() {
	for (let c = 0; c < brickColumnCount; c++) {
		for (let r = 0; r < brickRowCount; r++) {
			const b = bricks[c][r];
			if (!b.alive) continue;
			if (
				ballX + ballRadius > b.x &&
				ballX - ballRadius < b.x + brickWidth &&
				ballY + ballRadius > b.y &&
				ballY - ballRadius < b.y + brickHeight
			) {
				ballDY = -ballDY;
				b.alive = false;
				score++;
				if (score === brickRowCount * brickColumnCount) {
					state = State.Won;
				}
			}
		}
	}
}

function update() {
	// Move ball
	ballX += ballDX;
	ballY += ballDY;

	// Wall bounces (left/right)
	if (ballX - ballRadius < 0 || ballX + ballRadius > W) {
		ballDX = -ballDX;
		ballX = Math.max(ballRadius, Math.min(W - ballRadius, ballX));
	}

	// Top wall
	if (ballY - ballRadius < 0) {
		ballDY = -ballDY;
		ballY = ballRadius;
	}

	// Paddle collision
	if (
		ballDY > 0 &&
		ballY + ballRadius >= paddleY &&
		ballY + ballRadius <= paddleY + paddleHeight + ballDY &&
		ballX >= paddleX &&
		ballX <= paddleX + paddleWidth
	) {
		// Vary angle based on where ball hits paddle
		const hitPos = (ballX - paddleX) / paddleWidth; // 0..1
		const angle = -Math.PI / 3 + hitPos * (-Math.PI / 6 * 2 + Math.PI / 3 * 2);
		const speed = Math.sqrt(ballDX * ballDX + ballDY * ballDY);
		ballDX = speed * Math.cos(angle - Math.PI / 2);
		ballDY = -Math.abs(speed * Math.sin(angle - Math.PI / 2));
		ballY = paddleY - ballRadius;
	}

	// Bottom — lose life
	if (ballY + ballRadius > H) {
		lives--;
		if (lives <= 0) {
			state = State.GameOver;
		} else {
			resetBall();
		}
	}

	collisionDetection();
}

// ── Input ────────────────────────────────────────────────────────────
function handleInput(gamepad?: Gamepad | null) {
	if (!gamepad) return;

	if (state === State.Playing) {
		if (gamepad.buttons[Button.Left].pressed) {
			paddleX = Math.max(0, paddleX - paddleSpeed);
		}
		if (gamepad.buttons[Button.Right].pressed) {
			paddleX = Math.min(W - paddleWidth, paddleX + paddleSpeed);
		}
		// Analog stick
		const axis = gamepad.axes[0];
		if (Math.abs(axis) > 0.2) {
			paddleX = Math.max(0, Math.min(W - paddleWidth, paddleX + axis * paddleSpeed));
		}
	}

	if (state === State.Paused) {
		if (gamepad.buttons[Button.A].pressed) {
			state = State.Playing;
		}
	}

	if (state === State.GameOver || state === State.Won) {
		if (gamepad.buttons[Button.A].pressed) {
			start();
		}
	}
}

// Touch support — use screen canvas as event target
screen.addEventListener('touchstart', (e: TouchEvent) => {
	if (e.touches.length > 0) {
		touchX = e.touches[0].clientX;
	}
});
screen.addEventListener('touchmove', (e: TouchEvent) => {
	if (e.touches.length > 0 && state === State.Playing) {
		const newX = e.touches[0].clientX;
		if (touchX !== null) {
			paddleX = Math.max(0, Math.min(W - paddleWidth, paddleX + (newX - touchX)));
		}
		touchX = newX;
	}
});
screen.addEventListener('touchend', () => {
	touchX = null;
});

// Pause with + (Playing → Paused); exit with + from Paused/GameOver/Won
addEventListener('beforeunload', (event) => {
	if (state === State.Playing) {
		event.preventDefault();
		state = State.Paused;
	}
	// In Paused / GameOver / Won — allow the app to exit (don't preventDefault)
});

// ── FPS ──────────────────────────────────────────────────────────────
const fps = new FPS();

function drawFPS() {
	ctx.fillStyle = '#ecf0f1';
	ctx.font = '20px system-ui';
	ctx.fillText(`FPS: ${Math.round(fps.rate)}`, W - 104, 28);
}

// ── Main loop ────────────────────────────────────────────────────────
function step() {
	fps.tick();
	handleInput(navigator.getGamepads()[0]);

	if (state === State.Playing) {
		update();
		drawBackground();
		drawBricks();
		drawBall();
		drawPaddle();
		drawHUD();
		drawFPS();
	} else if (state === State.Paused) {
		drawBackground();
		drawBricks();
		drawBall();
		drawPaddle();
		drawHUD();
		drawFPS();
		drawOverlay('Paused', 'Press A to resume', '#ecf0f1');
	} else if (state === State.GameOver) {
		drawBackground();
		drawBricks();
		drawPaddle();
		drawHUD();
		drawFPS();
		drawOverlay('Game Over!', 'Press A to play again', '#e74c3c');
	} else if (state === State.Won) {
		drawBackground();
		drawHUD();
		drawFPS();
		drawOverlay('You Win!', 'Press A to play again', '#2ecc71');
	}

	requestAnimationFrame(step);
}

start();
requestAnimationFrame(step);
