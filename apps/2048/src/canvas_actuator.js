const bgColor = '#faf8ef';

// Tiles 2 and 4 are dark font
const tileFontColorDark = '#776e65';

// Every other tile is light
const tileFontColorLight = '#f9f6f2';

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

export class CanvasActuator {
	constructor() {
		/**
		 * @type CanvasRenderingContext2D
		 */
		this.ctx = Switch.screen.getContext('2d');

		// Draw background
		this.ctx.fillStyle = bgColor;
		this.ctx.fillRect(0, 0, this.ctx.canvas.width, this.ctx.canvas.height);

		// Draw title
		this.ctx.fillStyle = tileFontColorDark;
		this.ctx.font = 'bold 80px "Clear Sans"';
		this.ctx.fillText('2048', 20, 80);

		this.ctx.font = 'bold 30px "Clear Sans"';
		this.ctx.fillText('HOW TO PLAY:', 20, 120);

		this.gridSize = 600;
		this.gridSpacing = 18;
		this.tileSize = (this.gridSize - this.gridSpacing * (4 + 1)) / 4;
		this.gridX = this.ctx.canvas.width / 2 - this.gridSize / 2;
		this.gridY = this.ctx.canvas.height / 2 - this.gridSize / 2;

		this.score = 0;
	}
	actuate(grid, metadata) {
		var self = this;
		const { ctx } = this;

		setTimeout(function () {
			// Draw grid container
			ctx.fillStyle = '#bbada0';
			ctx.roundRect(
				self.gridX,
				self.gridY,
				self.gridSize,
				self.gridSize,
				8
			);
			ctx.fill();

			// Draw each cell
			grid.cells.forEach(function (column, x) {
				column.forEach(function (cell, y) {
					self.addTile(cell, x, y);
				});
			});

			self.updateScore(metadata.score);
			//self.updateBestScore(metadata.bestScore);

			//if (metadata.terminated) {
			//	if (metadata.over) {
			//		self.message(false); // You lose
			//	} else if (metadata.won) {
			//		self.message(true); // You win!
			//	}
			//}
		}, 0);
	}
	// Continues the game (both restart and keep playing)
	continueGame() {
		this.clearMessage();
	}
	addTile(tile, x, y) {
		const { ctx } = this;
		const tileX =
			this.gridX +
			this.gridSpacing +
			x * (this.tileSize + this.gridSpacing);
		const tileY =
			this.gridY +
			this.gridSpacing +
			y * (this.tileSize + this.gridSpacing);
		let fillStyle = 'rgba(238, 228, 218, 0.35)';
		if (tile?.value) {
			fillStyle = tileColors[tile.value] || tileColorMax;
		}
		ctx.fillStyle = fillStyle;
		ctx.roundRect(tileX, tileY, this.tileSize, this.tileSize, 6);
		ctx.fill();

		if (tile?.value) {
			const val = String(tile.value);
			const fontSize = tileFontSizes[tile.value] ?? 60;
			ctx.font = `bold ${fontSize}px "Clear Sans"`;
			ctx.fillStyle =
				tile.value > 4 ? tileFontColorLight : tileFontColorDark;
			const m = ctx.measureText(val);
			ctx.fillText(
				val,
				tileX + this.tileSize / 2 - m.width / 2,
				tileY + this.tileSize / 2 + m.height / 2
			);
		}
		//var wrapper = document.createElement('div');
		//var inner = document.createElement('div');
		//var position = tile.previousPosition || { x: tile.x, y: tile.y };
		//var positionClass = this.positionClass(position);

		// We can't use classlist because it somehow glitches when replacing classes
		//var classes = ['tile', 'tile-' + tile.value, positionClass];

		//if (tile.value > 2048) classes.push('tile-super');

		//this.applyClasses(wrapper, classes);

		//inner.classList.add('tile-inner');
		//inner.textContent = tile.value;

		//if (tile.previousPosition) {
		//	// Make sure that the tile gets rendered in the previous position first
		//	setTimeout(function () {
		//		classes[2] = self.positionClass({ x: tile.x, y: tile.y });
		//		self.applyClasses(wrapper, classes); // Update the position
		//	}, 0);
		//} else if (tile.mergedFrom) {
		//	classes.push('tile-merged');
		//	this.applyClasses(wrapper, classes);

		//	// Render the tiles that merged
		//	tile.mergedFrom.forEach(function (merged) {
		//		self.addTile(merged);
		//	});
		//} else {
		//	classes.push('tile-new');
		//	this.applyClasses(wrapper, classes);
		//}

		// Add the inner part of the tile to the wrapper
		//wrapper.appendChild(inner);

		// Put the tile on the board
		//this.tileContainer.appendChild(wrapper);
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

		//this.scoreContainer.textContent = this.score;
		ctx.fillStyle = tileFontColorLight;
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
		var type = won ? 'game-won' : 'game-over';
		var message = won ? 'You win!' : 'Game over!';

		this.messageContainer.classList.add(type);
		this.messageContainer.getElementsByTagName('p')[0].textContent =
			message;
	}
	clearMessage() {
		// IE only takes one value to remove at a time.
		//this.messageContainer.classList.remove('game-won');
		//this.messageContainer.classList.remove('game-over');
	}
}
