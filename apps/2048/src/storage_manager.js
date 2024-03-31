const key = '2048_state.json';

export class StorageManager {
	constructor() {
		let saved;
		try {
			saved = JSON.parse(localStorage.getItem(key));
		} catch (err) {
			console.debug('Error reading save data:', err);
		}
		this.bestScore = saved?.bestScore ?? 0;
		this.gameState = saved?.gameState ?? null;

		// Store game state on the SD card upon exit
		addEventListener('unload', this.onExit.bind(this));
	}
	// Best score getters/setters
	getBestScore() {
		return this.bestScore;
	}
	setBestScore(score) {
		this.bestScore = score;
	}
	// Game state getters/setters and clearing
	getGameState() {
		return this.gameState;
	}
	setGameState(gameState) {
		this.gameState = gameState;
	}
	clearGameState() {
		this.gameState = null;
	}
	onExit() {
		localStorage.setItem(key, JSON.stringify(this));
	}
}
