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

export class CanvasActuator {
	constructor() {
		this.lastUpdateAt = 0;

		/**
		 * @type CanvasRenderingContext2D
		 */
		this.ctx = Switch.screen.getContext('2d');

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

		this.score = 0;

		this.updateGridContainer = this.updateGridContainer.bind(this);
	}

	actuate(grid, metadata) {
		this.lastUpdateAt = Date.now();
		clearTimeout(this.updateGridContainerTimeoutId);
		this.updateGridContainer(grid, metadata);
		this.updateScore(metadata.score);
		//this.updateBestScore(metadata.bestScore);

		if (metadata.terminated) {
			if (metadata.over) {
				this.message(false); // You lose
			} else if (metadata.won) {
				this.message(true); // You win!
			}
		}
	}

	updateGridContainer(grid) {
		const delta = Date.now() - this.lastUpdateAt;

		this.drawGridContainer(grid);

		// Draw active cells
		for (let x = 0; x < grid.size; x++) {
			for (let y = 0; y < grid.size; y++) {
				const tile = grid.cells[x][y];
				if (tile) this.addTile(tile, delta);
			}
		}

		if (delta < tileMoveDuration) {
			this.updateGridContainerTimeoutId = setTimeout(
				this.updateGridContainer,
				0,
				grid
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
		this.clearMessage();
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
		const { ctx } = this;
		const scoreWidth = 200;
		const scoreHeight = 100;
		const scoreX = 1280 - scoreWidth;
		const scoreY = 100;

		ctx.fillStyle = '#bbada0';
		ctx.roundRect(scoreX, scoreY, scoreWidth, scoreHeight, 6);
		ctx.fill();

		var difference = score - this.score;
		this.score = score;

		ctx.fillStyle = fontColorLight;
		ctx.font = 'bold 30px "Clear Sans"';
		ctx.fillText(String(this.score), scoreX, scoreY + 30);

		//if (difference > 0) {
		//	var addition = document.createElement('div');
		//	addition.classList.add('score-addition');
		//	addition.textContent = '+' + difference;

		//	this.scoreContainer.appendChild(addition);
		//}
	}

	updateBestScore(bestScore) {
		this.bestContainer.textContent = bestScore;
	}

	message(won) {
		const message = won ? 'You win!' : 'Game over!';
	}

	clearMessage() {
		// IE only takes one value to remove at a time.
		//this.messageContainer.classList.remove('game-won');
		//this.messageContainer.classList.remove('game-over');
	}
}
