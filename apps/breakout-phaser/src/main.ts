/**
 * Breakout game using Phaser 3 on nx.js
 * Based on MDN 2D Breakout tutorial with Phaser
 *
 * DOM shim MUST be imported before Phaser.
 */
import './dom-shim';
import Phaser from 'phaser';
import { patchCanvas, SCREEN_W, SCREEN_H } from './dom-shim';
import { pollGamepad, setupTouchAdapter } from './input-adapter';
import { FPS } from './fps';

// ---------------------------------------------------------------------------
// Procedural texture generation
// ---------------------------------------------------------------------------

function makeTexture(
	scene: Phaser.Scene,
	key: string,
	w: number,
	h: number,
	draw: (ctx: OffscreenCanvasRenderingContext2D, w: number, h: number) => void,
) {
	const c = new OffscreenCanvas(w, h);
	const ctx = c.getContext('2d')!;
	draw(ctx, w, h);
	// Add to Phaser texture manager
	const patched = patchCanvas(c as any);
	scene.textures.addCanvas(key, patched);
}

// ---------------------------------------------------------------------------
// Game constants
// ---------------------------------------------------------------------------

const COLS = 10;
const ROWS = 5;
const BRICK_W = 100;
const BRICK_H = 30;
const BRICK_PAD = 10;
const BRICK_OFFSET_X = (SCREEN_W - COLS * (BRICK_W + BRICK_PAD) + BRICK_PAD) / 2;
const BRICK_OFFSET_Y = 80;

const PADDLE_W = 120;
const PADDLE_H = 20;
const PADDLE_SPEED = 600;

const BALL_R = 10;
const BALL_SPEED = 400;

const COLORS = [0xff4444, 0xff8844, 0xffcc44, 0x44cc44, 0x4488ff];

// ---------------------------------------------------------------------------
// Breakout Scene
// ---------------------------------------------------------------------------

class BreakoutScene extends Phaser.Scene {
	ball!: Phaser.Physics.Arcade.Sprite;
	paddle!: Phaser.Physics.Arcade.Sprite;
	bricks!: Phaser.Physics.Arcade.StaticGroup;
	cursors!: Phaser.Types.Input.Keyboard.CursorKeys;
	scoreText!: Phaser.GameObjects.Text;
	livesText!: Phaser.GameObjects.Text;
	infoText!: Phaser.GameObjects.Text;
	score = 0;
	lives = 3;
	started = false;

	constructor() {
		super({ key: 'Breakout' });
	}

	preload() {
		// Generate textures procedurally
		makeTexture(this, 'ball', BALL_R * 2, BALL_R * 2, (ctx, w, h) => {
			ctx.fillStyle = '#ffffff';
			ctx.beginPath();
			ctx.arc(w / 2, h / 2, BALL_R, 0, Math.PI * 2);
			ctx.fill();
		});

		makeTexture(this, 'paddle', PADDLE_W, PADDLE_H, (ctx, w, h) => {
			ctx.fillStyle = '#00ccff';
			ctx.fillRect(0, 0, w, h);
		});

		for (let i = 0; i < ROWS; i++) {
			const color = COLORS[i % COLORS.length];
			const hex = '#' + color.toString(16).padStart(6, '0');
			makeTexture(this, `brick${i}`, BRICK_W, BRICK_H, (ctx, w, h) => {
				ctx.fillStyle = hex;
				ctx.fillRect(0, 0, w, h);
			});
		}
	}

	create() {
		this.score = 0;
		this.lives = 3;
		this.started = false;

		// Paddle
		this.paddle = this.physics.add.sprite(SCREEN_W / 2, SCREEN_H - 50, 'paddle');
		this.paddle.setImmovable(true);
		this.paddle.body!.allowGravity = false;
		(this.paddle.body as Phaser.Physics.Arcade.Body).setCollideWorldBounds(true);

		// Ball
		this.ball = this.physics.add.sprite(SCREEN_W / 2, SCREEN_H - 80, 'ball');
		this.ball.setCollideWorldBounds(true);
		this.ball.setBounce(1, 1);
		this.ball.body!.allowGravity = false;

		// Enable world bounds collision event
		(this.ball.body as Phaser.Physics.Arcade.Body).onWorldBounds = true;
		this.physics.world.on('worldbounds', (_body: any, _up: boolean, down: boolean) => {
			if (down) this.loseLife();
		});

		// Bricks
		this.bricks = this.physics.add.staticGroup();
		for (let row = 0; row < ROWS; row++) {
			for (let col = 0; col < COLS; col++) {
				const x = BRICK_OFFSET_X + col * (BRICK_W + BRICK_PAD) + BRICK_W / 2;
				const y = BRICK_OFFSET_Y + row * (BRICK_H + BRICK_PAD) + BRICK_H / 2;
				this.bricks.create(x, y, `brick${row}`);
			}
		}

		// Collisions
		this.physics.add.collider(this.ball, this.paddle, this.hitPaddle, undefined, this);
		this.physics.add.collider(this.ball, this.bricks, this.hitBrick, undefined, this);

		// Input
		this.cursors = this.input.keyboard!.createCursorKeys();

		// UI
		this.scoreText = this.add.text(20, 20, 'Score: 0', {
			fontSize: '24px',
			color: '#ffffff',
		});
		this.livesText = this.add.text(SCREEN_W - 150, 20, 'Lives: 3', {
			fontSize: '24px',
			color: '#ffffff',
		});
		this.infoText = this.add.text(SCREEN_W / 2, SCREEN_H / 2, 'Press SPACE / A to start', {
			fontSize: '32px',
			color: '#ffff00',
		}).setOrigin(0.5);
	}

	update() {
		// Gamepad polling
		pollGamepad();

		// Paddle movement
		if (this.cursors.left.isDown) {
			this.paddle.setVelocityX(-PADDLE_SPEED);
		} else if (this.cursors.right.isDown) {
			this.paddle.setVelocityX(PADDLE_SPEED);
		} else {
			this.paddle.setVelocityX(0);
		}

		// Launch ball
		if (!this.started) {
			this.ball.setPosition(this.paddle.x, this.paddle.y - 30);
			if (this.cursors.space?.isDown) {
				this.started = true;
				this.infoText.setVisible(false);
				this.ball.setVelocity(
					Phaser.Math.Between(-200, 200),
					-BALL_SPEED,
				);
			}
		}
	}

	hitPaddle(
		_ball: any,
		_paddle: any,
	) {
		const ball = _ball as Phaser.Physics.Arcade.Sprite;
		const paddle = _paddle as Phaser.Physics.Arcade.Sprite;
		// Angle based on hit position
		let diff = ball.x - paddle.x;
		ball.setVelocityX(diff * 5);
	}

	hitBrick(
		_ball: any,
		_brick: any,
	) {
		const brick = _brick as Phaser.Physics.Arcade.Sprite;
		brick.disableBody(true, true);
		this.score += 10;
		this.scoreText.setText(`Score: ${this.score}`);

		if (this.bricks.countActive() === 0) {
			this.infoText.setText('YOU WIN!').setVisible(true);
			this.ball.setVelocity(0, 0);
			this.started = false;
			this.time.delayedCall(3000, () => this.scene.restart());
		}
	}

	loseLife() {
		this.lives--;
		this.livesText.setText(`Lives: ${this.lives}`);
		if (this.lives <= 0) {
			this.infoText.setText('GAME OVER').setVisible(true);
			this.ball.setVelocity(0, 0);
			this.started = false;
			this.time.delayedCall(3000, () => this.scene.restart());
		} else {
			this.ball.setVelocity(0, 0);
			this.started = false;
			this.infoText.setText('Press SPACE / A to launch').setVisible(true);
		}
	}
}

// ---------------------------------------------------------------------------
// FPS overlay
// ---------------------------------------------------------------------------

const fps = new FPS();
const ctx = screen.getContext('2d');

// ---------------------------------------------------------------------------
// Boot Phaser
// ---------------------------------------------------------------------------

const patchedScreen = patchCanvas(screen as any);

const config: Phaser.Types.Core.GameConfig = {
	type: Phaser.CANVAS,
	canvas: patchedScreen,
	width: SCREEN_W,
	height: SCREEN_H,
	backgroundColor: '#1a1a2e',
	customEnvironment: true,
	physics: {
		default: 'arcade',
		arcade: {
			gravity: { x: 0, y: 0 },
			debug: false,
		},
	},
	scene: BreakoutScene,
	banner: false,
	audio: {
		noAudio: true,
	},
	input: {
		keyboard: true,
		mouse: false,
		touch: false,
		gamepad: false,
	},
};

const game = new Phaser.Game(config);

// Setup touch → pointer adapter
setupTouchAdapter();
