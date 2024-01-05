const stateUrl = new URL('2048_state.json', Switch.argv[0] || 'sdmc:/');

export class StorageManager {
	constructor() {
		let saved;
		try {
			saved = JSON.parse(
				new TextDecoder().decode(Switch.readFileSync(stateUrl))
			);
		} catch (err) {
			// ignore
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
		try {
			Switch.writeFileSync(stateUrl, JSON.stringify(this));
		} catch (err) {
			// ignore
		}
	}
}
