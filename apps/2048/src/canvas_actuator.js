import { ease, easeIn, easeInOut } from './easings';

const bgColor = '#faf8ef';
const gridContainerColor = '#bbada0';

const fontColorDark = '#776e65';
const fontColorLight = '#f9f6f2';
const fontColorScore = '#eee4da';

const emptyTileColor = 'rgba(238, 228, 218, 0.35)';

const tileColors = {
	2: '#eee4da',
	4: '#eee1c9',
	8: '#f3b27a',
	16: '#f69664',
	32: '#f77c5f',
	64: '#f75f3b',
	128: '#edd073',
	256: '#edcc62',
	512: '#edc950',
	1024: '#edc53f',
	2048: '#edc22e',
};

// After 2048, this color is used
const tileColorMax = '#3c3a33';

// The font size to use in the tile,
// based on the number of digits
const tileLengthFontSizes = {
	1: 60,
	2: 60,
	3: 52,
	4: 44,
	5: 36,
};

const tileMoveDuration = 100;
const scoreAnimationDuration = 600;
const messageFadeAnimationDuration = 800;

const scoreDiffMeasureCache = new Map();

export class CanvasActuator {
	constructor() {
		this.lastUpdateAt = 0;
		this.continueGameAt = 0;

		/**
		 * @type CanvasRenderingContext2D
		 */
		this.ctx = screen.getContext('2d');

		this.ctx.textAlign = 'center';
		this.ctx.textBaseline = 'middle';

		// Draw background
		this.ctx.fillStyle = bgColor;
		this.ctx.fillRect(0, 0, this.ctx.canvas.width, this.ctx.canvas.height);

		// Draw instructions text
		const instructionsTextX = 170;
		let instructionsTextY = 100;
		this.ctx.fillStyle = fontColorDark;
		this.ctx.font = 'bold 80px "Clear Sans"';
		this.ctx.fillText('2048', instructionsTextX, instructionsTextY);

		instructionsTextY += 100;
		this.ctx.font = 'bold 30px "Clear Sans"';
		this.ctx.fillText('HOW TO PLAY:', instructionsTextX, instructionsTextY);

		instructionsTextY += 50;
		this.ctx.font = '24px "Clear Sans"';
		this.ctx.fillText(
			'Use the D-pad or swipe the',
			instructionsTextX,
			instructionsTextY
		);
		instructionsTextY += 30;
		this.ctx.fillText(
			'screen to move the tiles.',
			instructionsTextX,
			instructionsTextY
		);

		instructionsTextY += 60;
		this.ctx.fillText(
			'Tiles with the same',
			instructionsTextX,
			instructionsTextY
		);
		instructionsTextY += 30;
		this.ctx.fillText(
			'number merge into one',
			instructionsTextX,
			instructionsTextY
		);
		instructionsTextY += 30;
		this.ctx.fillText(
			'when they touch.',
			instructionsTextX,
			instructionsTextY
		);

		instructionsTextY += 60;
		this.ctx.fillText(
			'Add them up to reach 2048!',
			instructionsTextX,
			instructionsTextY
		);

		this.gridSize = 600;
		this.gridSpacing = 18;
		this.tileSize = (this.gridSize - this.gridSpacing * (4 + 1)) / 4;
		this.gridX = this.ctx.canvas.width / 2 - this.gridSize / 2;
		this.gridY = this.ctx.canvas.height / 2 - this.gridSize / 2;
		this.sidebarWidth = (this.ctx.canvas.width - this.gridSize) / 2;
		this.scoreBoxWidth = this.sidebarWidth * 0.5;
		this.scoreBoxHeight = 100;

		this.score = 0;
		this.scoreAnimations = new Map();

		this.updateGridContainer = this.updateGridContainer.bind(this);
		this.drawScore = this.drawScore.bind(this);
	}

	actuate(grid, metadata) {
		this.lastUpdateAt = Date.now();
		cancelAnimationFrame(this.updateGridContainerTimeoutId);
		this.updateGridContainer(grid, metadata);
		this.updateScore(metadata.score);
		this.drawBestScore(metadata.bestScore);
	}

	updateGridContainer(grid, metadata) {
		const delta = Date.now() - this.lastUpdateAt;

		this.drawGridContainer(grid);

		// Draw active cells
		for (let x = 0; x < grid.size; x++) {
			for (let y = 0; y < grid.size; y++) {
				const tile = grid.cells[x][y];
				if (tile) this.addTile(tile, delta);
			}
		}

		let animationDuration = tileMoveDuration;

		if (metadata.terminated && this.continueGameAt === 0) {
			animationDuration = messageFadeAnimationDuration;
			if (metadata.over) {
				this.message(false, delta); // You lose
			} else if (metadata.won) {
				this.message(true, delta); // You win!
			}
		}

		if (delta < animationDuration) {
			this.updateGridContainerTimeoutId = requestAnimationFrame(() => {
				this.updateGridContainer(grid, metadata);
			});
		}
	}

	drawGridContainer(grid) {
		const { ctx } = this;

		// Draw grid container
		ctx.beginPath();
		ctx.roundRect(this.gridX, this.gridY, this.gridSize, this.gridSize, 8);
		ctx.fillStyle = gridContainerColor;
		ctx.fill();

		// Draw empty cells
		for (let x = 0; x < grid.size; x++) {
			for (let y = 0; y < grid.size; y++) {
				const tilePos = this.tilePosition(x, y);
				this.drawTile(null, tilePos.x, tilePos.y);
			}
		}
	}

	// Continues the game (both restart and keep playing)
	continueGame() {
		this.continueGameAt = Date.now();
	}

	tilePosition(x, y) {
		return {
			x:
				this.gridX +
				this.gridSpacing +
				x * (this.tileSize + this.gridSpacing),
			y:
				this.gridY +
				this.gridSpacing +
				y * (this.tileSize + this.gridSpacing),
		};
	}

	addTile(tile, delta) {
		if (tile.previousPosition) {
			// Render the (potentially) moving tile at its current location
			const oldPos = this.tilePosition(
				tile.previousPosition.x,
				tile.previousPosition.y
			);
			const tilePos = this.tilePosition(tile.x, tile.y);
			const t = Math.min(delta / tileMoveDuration, 1);
			const e = easeInOut(t);
			const dx = tilePos.x - oldPos.x;
			const dy = tilePos.y - oldPos.y;
			const x = oldPos.x + dx * e.y;
			const y = oldPos.y + dy * e.y;
			this.drawTile(tile, x, y);
		} else if (tile.mergedFrom) {
			const t = Math.min(delta / tileMoveDuration, 1);
			if (t === 1) {
				// Movement animation completed, so render the merged cell
				// TODO: make it "pop"
				const tilePos = this.tilePosition(tile.x, tile.y);
				this.drawTile(tile, tilePos.x, tilePos.y);
			} else {
				// Render the tiles that merged
				for (const mergedTile of tile.mergedFrom) {
					this.addTile(mergedTile, delta);
				}
			}
		} else {
			// TODO: "appear" animation, delayed by 100ms
			const tilePos = this.tilePosition(tile.x, tile.y);
			this.drawTile(tile, tilePos.x, tilePos.y);
		}
	}

	drawTile(tile, x, y) {
		const { ctx } = this;
		let fillStyle = emptyTileColor;
		if (tile?.value) {
			fillStyle = tileColors[tile.value] || tileColorMax;
		}
		ctx.beginPath();
		ctx.roundRect(x, y, this.tileSize, this.tileSize, 6);
		ctx.fillStyle = fillStyle;
		ctx.fill();

		if (tile?.value) {
			const val = String(tile.value);
			const fontSize = tileLengthFontSizes[val.length];
			ctx.font = `bold ${fontSize}px "Clear Sans"`;
			ctx.fillStyle = tile.value > 4 ? fontColorLight : fontColorDark;
			ctx.fillText(val, x + this.tileSize / 2, y + this.tileSize / 2);
		}
	}

	updateScore(score) {
		const difference = score - this.score;
		this.score = score;
		if (difference > 0) {
			this.scoreAnimations.set(Date.now(), difference);
		}
		this.drawScore();
	}

	drawScore() {
		const { ctx } = this;
		const scoreWidth = this.scoreBoxWidth;
		const scoreHeight = this.scoreBoxHeight;
		const scoreX =
			ctx.canvas.width - this.sidebarWidth + this.scoreBoxWidth / 2;
		const scoreY = 80;

		ctx.fillStyle = bgColor;
		ctx.fillRect(scoreX, scoreY - 80, scoreWidth, scoreHeight + 80);

		ctx.beginPath();
		ctx.fillStyle = '#bbada0';
		ctx.roundRect(scoreX, scoreY, scoreWidth, scoreHeight, 6);
		ctx.fill();

		const scoreTextX = scoreX + scoreWidth / 2;

		ctx.fillStyle = fontColorScore;
		ctx.font = 'bold 24px "Clear Sans"';
		ctx.fillText('SCORE', scoreTextX, scoreY + 30);

		ctx.fillStyle = fontColorLight;
		ctx.font = 'bold 36px "Clear Sans"';
		const scoreStr = String(this.score);
		const scoreTextY = 68 + scoreY;
		ctx.fillText(scoreStr, scoreTextX, scoreTextY);

		const now = Date.now();

		for (const [startedAt, difference] of this.scoreAnimations.entries()) {
			const diff = now - startedAt;
			const t = Math.min(diff / scoreAnimationDuration, 1);
			const e = easeIn(t);
			ctx.fillStyle = `rgba(119, 110, 101, ${1 - e.y})`;
			const scoreDiff = `+${difference}`;
			const y = scoreTextY - scoreHeight * e.y;
			const scoreTextX = scoreX + scoreWidth / 2;
			ctx.fillText(scoreDiff, scoreTextX, y);
			if (t === 1) {
				this.scoreAnimations.delete(startedAt);
			}
		}

		if (this.scoreAnimations.size > 0) {
			requestAnimationFrame(this.drawScore);
		}
	}

	drawBestScore(bestScore) {
		const { ctx } = this;
		const scoreWidth = this.scoreBoxWidth;
		const scoreHeight = this.scoreBoxHeight;
		const scoreX =
			ctx.canvas.width - this.sidebarWidth + this.scoreBoxWidth / 2;
		const scoreY = 220;

		ctx.fillStyle = bgColor;
		ctx.fillRect(scoreX, scoreY, scoreWidth, scoreHeight);

		ctx.beginPath();
		ctx.fillStyle = '#bbada0';
		ctx.roundRect(scoreX, scoreY, scoreWidth, scoreHeight, 6);
		ctx.fill();

		const scoreTextX = scoreX + scoreWidth / 2;

		ctx.fillStyle = fontColorScore;
		ctx.font = 'bold 24px "Clear Sans"';
		ctx.fillText('BEST', scoreTextX, scoreY + 30);

		ctx.fillStyle = fontColorLight;
		ctx.font = 'bold 36px "Clear Sans"';
		const scoreStr = String(bestScore);
		const scoreTextY = 68 + scoreY;
		ctx.fillText(scoreStr, scoreTextX, scoreTextY);
	}

	message(won, delta) {
		const t = Math.min(delta / messageFadeAnimationDuration, 1);
		const e = ease(t);

		this.ctx.beginPath();
		this.ctx.fillStyle = `rgba(255, 255, 255, ${e.y / 2})`;
		this.ctx.roundRect(
			this.gridX,
			this.gridY,
			this.gridSize,
			this.gridSize,
			8
		);
		this.ctx.fill();

		this.ctx.font = 'bold 64px "Clear Sans"';
		this.ctx.fillStyle = `rgba(119, 110, 101, ${e.y})`;
		const message = won ? 'You win!' : 'Game over!';
		const x = this.gridX + this.gridSize / 2;
		const y = this.gridY + this.gridSize / 2;
		this.ctx.fillText(message, x, y);
	}
}
