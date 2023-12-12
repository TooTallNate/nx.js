import { FPS } from './fps';
import { Hid } from 'nxjs-constants';
const { Button } = Hid;

interface Position {
	x: number;
	y: number;
}

interface SnakeBodyPart extends Position {
	direction: Direction;
}

enum Direction {
	Up,
	Down,
	Left,
	Right,
}

enum State {
	Playing,
	Paused,
	Gameover,
}

const ctx = screen.getContext('2d');

const snakeBody: SnakeBodyPart[] = [];
const gridSize = 16;
const boardWidth = 76;
const boardHeight = 40;
const boardX = screen.width / 2 - (boardWidth * gridSize) / 2;
const boardY = screen.height / 2 - (boardHeight * gridSize) / 2;
const updateRate = 100;
let updatedAt = 0;
let food: Position = { x: 0, y: 0 };
let direction: Direction = Direction.Right;
let directionChange: Direction | undefined;
let interval: number = 0;
let state: State;
let score = 0;

function start() {
	makeFood();
	score = 0;
	snakeBody.length = 0;
	snakeBody.push(
		{ x: 5, y: 5, direction: Direction.Right },
		{ x: 6, y: 5, direction: Direction.Right },
		{ x: 7, y: 5, direction: Direction.Right },
		{ x: 8, y: 5, direction: Direction.Right }
	);
	direction = Direction.Right;
	updateScore();
	play();
}

function pause() {
	state = State.Paused;
	clearInterval(interval);

	const boxWidth = 400;
	const boxHeight = 200;
	const boxX = screen.width / 2 - boxWidth / 2;
	const boxY = screen.height / 2 - boxHeight / 2;
	ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
	ctx.fillRect(boxX, boxY, boxWidth, boxHeight);
	// TODO: use `ctx.measureText()` to properly center text
	ctx.fillStyle = 'white';
	ctx.font = '40px system-ui';
	let textY = boxY + 80;
	ctx.fillText('Paused!', boxX + 125, textY);
	ctx.font = '24px system-ui';
	textY += 60;
	ctx.fillText('Press + to resume...', boxX + 80, textY);
}

function play() {
	state = State.Playing;
	interval = setInterval(update, updateRate);
	updatedAt = Date.now();
	draw();
}

function draw() {
	// Reset board
	ctx.beginPath();
	ctx.rect(boardX, boardY, boardWidth * gridSize, boardHeight * gridSize);
	ctx.fillStyle = 'rgb(0, 0, 70)';
	ctx.fill();
	ctx.strokeStyle = 'white';
	ctx.stroke();

	// Draw food
	ctx.beginPath();
	ctx.arc(
		boardX + gridSize / 2 + food.x * gridSize,
		boardY + gridSize / 2 + food.y * gridSize,
		gridSize / 2,
		0,
		2 * Math.PI
	);
	ctx.fillStyle = 'green';
	ctx.fill();

	// Draw snake
	ctx.beginPath();
	const now = Date.now();
	const diff = now - updatedAt;
	const pos = (diff / updateRate) * gridSize;

	// Snake tail
	const tail = snakeBody[0];
	if (tail.direction === Direction.Right) {
		ctx.rect(
			pos + boardX + tail.x * gridSize,
			boardY + tail.y * gridSize,
			gridSize - pos,
			gridSize
		);
	} else if (tail.direction === Direction.Left) {
		ctx.rect(
			boardX + tail.x * gridSize,
			boardY + tail.y * gridSize,
			gridSize - pos,
			gridSize
		);
	} else if (tail.direction === Direction.Up) {
		ctx.rect(
			boardX + tail.x * gridSize,
			boardY + tail.y * gridSize,
			gridSize,
			gridSize - pos
		);
	} else {
		ctx.rect(
			boardX + tail.x * gridSize,
			pos + boardY + tail.y * gridSize,
			gridSize,
			gridSize - pos
		);
	}

	// Snake body
	for (let i = 1; i < snakeBody.length - 1; i++) {
		ctx.rect(
			boardX + snakeBody[i].x * gridSize,
			boardY + snakeBody[i].y * gridSize,
			gridSize,
			gridSize
		);
	}

	// Snake head
	const head = snakeBody[snakeBody.length - 1];
	if (head.direction === Direction.Right) {
		ctx.rect(
			boardX + head.x * gridSize,
			boardY + head.y * gridSize,
			pos,
			gridSize
		);
	} else if (head.direction === Direction.Left) {
		ctx.rect(
			gridSize - pos + boardX + head.x * gridSize,
			boardY + head.y * gridSize,
			pos,
			gridSize
		);
	} else if (head.direction === Direction.Up) {
		ctx.rect(
			boardX + head.x * gridSize,
			gridSize - pos + boardY + head.y * gridSize,
			gridSize,
			pos
		);
	} else {
		ctx.rect(
			boardX + head.x * gridSize,
			boardY + head.y * gridSize,
			gridSize,
			pos
		);
	}
	ctx.fillStyle = 'red';
	ctx.fill();
}

function update() {
	updatedAt = Date.now();

	// Only allow the direction to change once per update.
	// This is to avoid an issue where very quick direction
	// changes might make the snake eat itself inadvertently.
	if (typeof directionChange !== 'undefined') {
		direction = directionChange;
		directionChange = undefined;
	}

	const oldHead = snakeBody[snakeBody.length - 1];
	oldHead.direction = direction;

	// Move snake
	const head = { ...oldHead, direction };
	if (direction === Direction.Up) {
		head.y--;
	} else if (direction === Direction.Down) {
		head.y++;
	} else if (direction === Direction.Left) {
		head.x--;
	} else if (direction === Direction.Right) {
		head.x++;
	}

	// if `head` is past the edge, or within the snake body - game over
	if (
		head.x < 0 ||
		head.y < 0 ||
		head.x >= boardWidth ||
		head.y >= boardHeight ||
		isWithinSnake(head)
	) {
		gameOver();
		return;
	}

	snakeBody.push(head);

	// if `head` matches the food position, then eat it
	if (positionsEqual(head, food)) {
		score++;
		makeFood();
		updateScore();
	} else {
		snakeBody.shift();
	}
}

function positionsEqual(a: Position, b: Position) {
	return a.x === b.x && a.y === b.y;
}

function isWithinSnake(pos: Position) {
	return snakeBody.some((s) => positionsEqual(pos, s));
}

function makeFood(): void {
	food = {
		x: Math.floor(Math.random() * boardWidth),
		y: Math.floor(Math.random() * boardHeight),
	};
	if (isWithinSnake(food)) {
		// Inside of snake body, so try again
		makeFood();
	}
}

function gameOver() {
	state = State.Gameover;
	clearInterval(interval);
	const boxWidth = 500;
	const boxHeight = 300;
	const boxX = screen.width / 2 - boxWidth / 2;
	const boxY = screen.height / 2 - boxHeight / 2;
	ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
	ctx.fillRect(boxX, boxY, boxWidth, boxHeight);
	ctx.fillStyle = 'red';
	// TODO: use `ctx.measureText()` to properly center text
	ctx.font = '50px system-ui';
	let textY = boxY + 80;
	ctx.fillText('Game Over!', boxX + 100, textY);
	ctx.fillStyle = 'white';
	ctx.font = '36px system-ui';
	textY += 60;
	ctx.fillText(`Your score: ${score}`, boxX + 140, textY);
	ctx.font = '24px system-ui';
	textY += 60;
	ctx.fillText('Press A to try again...', boxX + 125, textY);
	textY += 50;
	ctx.fillText('Press + to exit...', boxX + 150, textY);
}

Switch.addEventListener('buttondown', (event) => {
	if (state === State.Playing) {
		if (event.detail & Button.Plus) {
			event.preventDefault();
			pause();
		} else if (!directionChange) {
			if (event.detail & Button.AnyLeft) {
				if (direction !== Direction.Right) {
					directionChange = Direction.Left;
				}
			} else if (event.detail & Button.AnyUp) {
				if (direction !== Direction.Down) {
					directionChange = Direction.Up;
				}
			} else if (event.detail & Button.AnyRight) {
				if (direction !== Direction.Left) {
					directionChange = Direction.Right;
				}
			} else if (event.detail & Button.AnyDown) {
				if (direction !== Direction.Up) {
					directionChange = Direction.Down;
				}
			}
		}
	} else if (state === State.Paused) {
		if (event.detail & Button.Plus) {
			event.preventDefault();
			play();
		}
	} else if (state === State.Gameover) {
		if (event.detail & Button.A) {
			start();
		}
	}
});

function updateScore() {
	ctx.fillStyle = 'black';
	ctx.fillRect(32, 0, 250, 30);

	ctx.fillStyle = 'white';
	ctx.font = '20px system-ui';
	ctx.fillText(`Score: ${score}`, 32, 26);
}

const fps = new FPS();

fps.addEventListener('update', () => {
	ctx.fillStyle = 'black';
	ctx.fillRect(screen.width - 104, 0, 90, 26);

	ctx.fillStyle = 'white';
	ctx.font = '20px system-ui';
	ctx.fillText(`FPS: ${Math.round(fps.rate)}`, screen.width - 104, 26);
});

function step() {
	fps.tick();
	if (state === State.Playing) draw();
	requestAnimationFrame(step);
}
requestAnimationFrame(step);

start();
