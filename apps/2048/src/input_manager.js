import { Hid } from 'nxjs-constants';

const { Button } = Hid;

export class InputManager {
	constructor() {
		this.events = {};
		this.listen();
	}
	on(event, callback) {
		if (!this.events[event]) {
			this.events[event] = [];
		}
		this.events[event].push(callback);
	}
	emit(event, data) {
		var callbacks = this.events[event];
		if (callbacks) {
			callbacks.forEach(function (callback) {
				callback(data);
			});
		}
	}
	listen() {
		var self = this;

		//var map = {
		//    38: 0,
		//    39: 1,
		//    40: 2,
		//    37: 3,
		//    75: 0,
		//    76: 1,
		//    74: 2,
		//    72: 3,
		//    87: 0,
		//    68: 1,
		//    83: 2,
		//    65: 3 // A
		//};

		//// Respond to direction keys
		//document.addEventListener("keydown", function (event) {
		//    var modifiers = event.altKey || event.ctrlKey || event.metaKey ||
		//        event.shiftKey;
		//    var mapped = map[event.which];

		//    if (!modifiers) {
		//        if (mapped !== undefined) {
		//            event.preventDefault();
		//            self.emit("move", mapped);
		//        }
		//    }

		//    // R key restarts the game
		//    if (!modifiers && event.which === 82) {
		//        self.restart.call(self, event);
		//    }
		//});

		// Respond to button presses
		//this.bindButtonPress(".retry-button", this.restart);
		//this.bindButtonPress(".restart-button", this.restart);
		//this.bindButtonPress(".keep-playing-button", this.keepPlaying);

		// Respond to swipe events
		var touchStartClientX, touchStartClientY;
		Switch.addEventListener('touchstart', function (event) {
			if (event.touches.length > 1) {
				return; // Ignore if touching with more than 1 finger
			}

			touchStartClientX = event.touches[0].clientX;
			touchStartClientY = event.touches[0].clientY;

			event.preventDefault();
		});

		Switch.addEventListener('touchmove', function (event) {
			event.preventDefault();
		});

		Switch.addEventListener('touchend', function (event) {
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
				self.emit(
					'move',
					absDx > absDy ? (dx > 0 ? 1 : 3) : dy > 0 ? 2 : 0
				);
			}
		});

		Switch.addEventListener('buttondown', (event) => {
			if (event.detail & Button.AnyLeft) {
				self.emit('move', 3);
			} else if (event.detail & Button.AnyRight) {
				self.emit('move', 1);
			} else if (event.detail & Button.AnyUp) {
				self.emit('move', 0);
			} else if (event.detail & Button.AnyDown) {
				self.emit('move', 2);
			}
		});
	}
	restart(event) {
		event.preventDefault();
		this.emit('restart');
	}
	keepPlaying(event) {
		event.preventDefault();
		this.emit('keepPlaying');
	}
	//bindButtonPress(selector, fn) {
	//    var button = document.querySelector(selector);
	//    button.addEventListener("click", fn.bind(this));
	//    button.addEventListener(this.eventTouchend, fn.bind(this));
	//}
}
