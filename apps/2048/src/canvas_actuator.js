import { ease, easeIn, easeInOut } from './easings';

const bgColor = '#faf8ef';
const gridContainerColor = '#bbada0';

const fontColorDark = '#776e65';
const fontColorLight = '#f9f6f2';

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

const tileFontSizes = {
	2: 60,
	4: 60,
	8: 60,
	16: 60,
};

// After 2048, this color is used
const tileColorMax = '#3c3a33';

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

		// Draw background
		this.ctx.fillStyle = bgColor;
		this.ctx.fillRect(0, 0, this.ctx.canvas.width, this.ctx.canvas.height);

		// Draw title
		this.ctx.fillStyle = fontColorDark;
		this.ctx.font = 'bold 80px "Clear Sans"';
		this.ctx.fillText('2048', 60, 80);

		this.ctx.font = 'bold 30px "Clear Sans"';
		this.ctx.fillText('HOW TO PLAY:', 60, 140);

		this.ctx.font = '24px "Clear Sans"';
		this.ctx.fillText('Use the D-pad or swipe the', 20, 190);
		this.ctx.fillText('screen to move the tiles.', 20, 220);
		this.ctx.fillText('Tiles with the same', 20, 270);
		this.ctx.fillText('number merge into one', 20, 300);
		this.ctx.fillText('when they touch.', 20, 330);
		this.ctx.fillText('Add them up to reach 2048!', 20, 380);

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
		clearTimeout(this.updateGridContainerTimeoutId);
		this.updateGridContainer(grid, metadata);
		this.updateScore(metadata.score);
		//this.updateBestScore(metadata.bestScore);
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
			this.updateGridContainerTimeoutId = setTimeout(
				this.updateGridContainer,
				0,
				grid,
				metadata
			);
		}
	}

	drawGridContainer(grid) {
		// Draw grid container
		this.ctx.fillStyle = gridContainerColor;
		this.ctx.roundRect(
			this.gridX,
			this.gridY,
			this.gridSize,
			this.gridSize,
			8
		);
		this.ctx.fill();

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
		ctx.fillStyle = fillStyle;
		ctx.roundRect(x, y, this.tileSize, this.tileSize, 6);
		ctx.fill();

		if (tile?.value) {
			const val = String(tile.value);
			const fontSize = tileFontSizes[tile.value] ?? 60;
			ctx.font = `bold ${fontSize}px "Clear Sans"`;
			ctx.fillStyle = tile.value > 4 ? fontColorLight : fontColorDark;
			const m = ctx.measureText(val);
			ctx.fillText(
				val,
				x + this.tileSize / 2 - m.width / 2,
				y + this.tileSize / 2 + m.height / 2
			);
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

		ctx.fillStyle = '#bbada0';
		ctx.roundRect(scoreX, scoreY, scoreWidth, scoreHeight, 6);
		ctx.fill();

		ctx.fillStyle = fontColorLight;
		ctx.font = 'bold 36px "Clear Sans"';
		const scoreStr = String(this.score);
		const scoreMeasure = ctx.measureText(scoreStr);
		const scoreTextX = scoreX + (scoreWidth / 2 - scoreMeasure.width / 2);
		const scoreTextY =
			10 + scoreY + (scoreHeight / 2 + scoreMeasure.height / 2);
		ctx.fillText(scoreStr, scoreTextX, scoreTextY);

		const now = Date.now();

		for (const [startedAt, difference] of this.scoreAnimations.entries()) {
			const diff = now - startedAt;
			const t = Math.min(diff / scoreAnimationDuration, 1);
			const e = easeIn(t);
			ctx.fillStyle = `rgba(119, 110, 101, ${1 - e.y})`;
			const scoreDiff = `+${difference}`;
			let scoreDiffMeasure = scoreDiffMeasureCache.get(scoreDiff);
			if (!scoreDiffMeasure) {
				scoreDiffMeasure = ctx.measureText(scoreDiff);
				scoreDiffMeasureCache.get(scoreDiff, scoreDiffMeasure);
			}
			const y = scoreTextY - scoreHeight * e.y;
			const scoreTextX =
				scoreX + (scoreWidth / 2 - scoreDiffMeasure.width / 2);
			ctx.fillText(scoreDiff, scoreTextX, y);
			if (t === 1) {
				this.scoreAnimations.delete(startedAt);
			}
		}

		if (this.scoreAnimations.size > 0) {
			setTimeout(this.drawScore, 0);
		}
	}

	//updateBestScore(bestScore) {
	//	this.bestContainer.textContent = bestScore;
	//}

	message(won, delta) {
		const t = Math.min(delta / messageFadeAnimationDuration, 1);
		const e = ease(t);

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
		const m = this.ctx.measureText(message);
		const x = this.gridX + (this.gridSize / 2 - m.width / 2);
		const y = this.gridY + (this.gridSize / 2 + m.height / 2);
		this.ctx.fillText(message, x, y);
	}
}
