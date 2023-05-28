import { FPS } from './fps';
import { Button } from 'nxjs-constants';

type Position = [number, number];
type Direction = 'up' | 'down' | 'left' | 'right';
type State = 'playing' | 'paused' | 'gameover';

const ctx = Switch.screen.getContext('2d');

const snakeBody: Position[] = [];
const gridSize = 16;
const boardWidth = 76;
const boardHeight = 40;
const boardX = Switch.screen.width / 2 - (boardWidth * gridSize) / 2;
const boardY = Switch.screen.height / 2 - (boardHeight * gridSize) / 2;
let food: Position = [0, 0];
let direction: Direction = 'right';
let directionChange: Direction | undefined;
let interval: number = 0;
let state: State;
let score = 0;

function start() {
	makeFood();
	score = 0;
	snakeBody.length = 0;
	snakeBody.push([5, 5], [6, 5], [7, 5]);
	direction = 'right';
	updateScore();
	play();
}

function pause() {
	state = 'paused';
	clearInterval(interval);

	const boxWidth = 400;
	const boxHeight = 200;
	const boxX = Switch.screen.width / 2 - boxWidth / 2;
	const boxY = Switch.screen.height / 2 - boxHeight / 2;
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
	state = 'playing';
	interval = setInterval(update, 100);
	draw();
}

function draw() {
	// Draw border
	// TODO: use `strokeRect()` instead
	ctx.fillStyle = 'white';
	ctx.fillRect(
		boardX - 1,
		boardY - 1,
		boardWidth * gridSize + 2,
		boardHeight * gridSize + 2
	);

	// Reset board
	ctx.fillStyle = 'rgb(0, 0, 70)';
	ctx.fillRect(boardX, boardY, boardWidth * gridSize, boardHeight * gridSize);

	// Draw food
	ctx.fillStyle = 'green';
	ctx.fillRect(
		boardX + food[0] * gridSize,
		boardY + food[1] * gridSize,
		gridSize,
		gridSize
	);

	// Draw snake
	ctx.fillStyle = 'red';
	for (const [x, y] of snakeBody) {
		ctx.fillRect(
			boardX + x * gridSize,
			boardY + y * gridSize,
			gridSize,
			gridSize
		);
	}
}

function update() {
	// Only allow the direction to change once per update.
	// This is to avoid an issue where very quick direction
	// changes might make the snake eat itself inadvertently.
	if (directionChange) {
		direction = directionChange;
		directionChange = undefined;
	}

	// Move snake
	const oldHead = snakeBody[snakeBody.length - 1];
	const head: Position = [oldHead[0], oldHead[1]];
	if (direction === 'up') {
		head[1]--;
	} else if (direction === 'down') {
		head[1]++;
	} else if (direction === 'left') {
		head[0]--;
	} else if (direction === 'right') {
		head[0]++;
	}

	// if `head` is past the edge, or within the snake body - game over
	if (
		head[0] < 0 ||
		head[1] < 0 ||
		head[0] >= boardWidth ||
		head[1] >= boardHeight ||
		isWithinSnake(head)
	) {
		gameOver();
		return;
	}

	snakeBody.push(head);

	// if `head` matches the food position, then eat it
	if (head[0] === food[0] && head[1] === food[1]) {
		score++;
		makeFood();
		updateScore();
	} else {
		snakeBody.shift();
	}

	draw();
}

function isWithinSnake(pos: Position) {
	return snakeBody.some(
		(snakePos) => pos[0] === snakePos[0] && pos[1] === snakePos[1]
	);
}

function makeFood(): void {
	food = [
		Math.floor(Math.random() * boardWidth),
		Math.floor(Math.random() * boardHeight),
	];
	if (isWithinSnake(food)) {
		// Inside of snake body, so try again
		makeFood();
	}
}

function gameOver() {
	state = 'gameover';
	clearInterval(interval);
	const score = snakeBody.length - 3;
	const boxWidth = 500;
	const boxHeight = 300;
	const boxX = Switch.screen.width / 2 - boxWidth / 2;
	const boxY = Switch.screen.height / 2 - boxHeight / 2;
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
	if (state === 'playing') {
		if (event.detail & Button.Plus) {
			event.preventDefault();
			pause();
		} else if (!directionChange) {
			if (event.detail & Button.AnyLeft) {
				if (direction !== 'right') {
					directionChange = 'left';
				}
			} else if (event.detail & Button.AnyUp) {
				if (direction !== 'down') {
					directionChange = 'up';
				}
			} else if (event.detail & Button.AnyRight) {
				if (direction !== 'left') {
					directionChange = 'right';
				}
			} else if (event.detail & Button.AnyDown) {
				if (direction !== 'up') {
					directionChange = 'down';
				}
			}
		}
	} else if (state === 'paused') {
		if (event.detail & Button.Plus) {
			event.preventDefault();
			play();
		}
	} else if (state === 'gameover') {
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
	ctx.fillRect(Switch.screen.width - 104, 0, 90, 26);

	ctx.fillStyle = 'white';
	ctx.font = '20px system-ui';
	ctx.fillText(`FPS: ${Math.round(fps.rate)}`, Switch.screen.width - 104, 26);
});

Switch.addEventListener('frame', fps.tick);

start();
