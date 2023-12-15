import { Hid } from 'nxjs-constants';

const { Button } = Hid;

export class InputManager {
	constructor(gameManager) {
		this.gameManager = gameManager;
		this.listen();
	}
	listen() {
		// Respond to swipe events
		var touchStartClientX, touchStartClientY;
		screen.addEventListener('touchstart', (event) => {
			if (event.touches.length > 1) {
				return; // Ignore if touching with more than 1 finger
			}

			touchStartClientX = event.touches[0].clientX;
			touchStartClientY = event.touches[0].clientY;

			event.preventDefault();
		});

		screen.addEventListener('touchmove', (event) => {
			event.preventDefault();
		});

		screen.addEventListener('touchend', (event) => {
			if (event.touches.length > 0) {
				return; // Ignore if still touching with one or more fingers
			}

			var touchEndClientX = event.changedTouches[0].clientX;
			var touchEndClientY = event.changedTouches[0].clientY;

			var dx = touchEndClientX - touchStartClientX;
			var absDx = Math.abs(dx);

			var dy = touchEndClientY - touchStartClientY;
			var absDy = Math.abs(dy);

			if (Math.max(absDx, absDy) > 10) {
				// (right : left) : (down : up)
				this.gameManager.move(
					absDx > absDy ? (dx > 0 ? 1 : 3) : dy > 0 ? 2 : 0
				);
			}
		});

		Switch.addEventListener('buttondown', (event) => {
			if (event.detail & Button.AnyLeft) {
				this.gameManager.move(3);
			} else if (event.detail & Button.AnyRight) {
				this.gameManager.move(1);
			} else if (event.detail & Button.AnyUp) {
				this.gameManager.move(0);
			} else if (event.detail & Button.AnyDown) {
				this.gameManager.move(2);
			} else if (event.detail & Button.Minus) {
				this.gameManager.restart();
			} else if (this.gameManager.won && !this.gameManager.keepPlaying) {
				if (event.detail & Button.Plus) {
					event.preventDefault();
					this.gameManager.continueGame();
				}
			}
		});
	}
}
