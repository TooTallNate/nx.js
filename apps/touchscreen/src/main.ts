import ColorHash from 'color-hash';

const hash = new ColorHash();
const ctx = Switch.screen.getContext('2d');

ctx.fillStyle = 'white';
ctx.font = '48px system-ui';
ctx.fillText('Touch the screen to draw...', 320, 350);

Switch.addEventListener(
	'touchstart',
	(e) => {
		ctx.fillStyle = 'black';
		ctx.fillRect(0, 0, Switch.screen.width, Switch.screen.height);
	},
	{ once: true }
);

function degreesToRadians(degrees: number) {
	return degrees * (Math.PI / 180);
}

Switch.addEventListener('touchmove', (e) => {
	for (const touch of e.changedTouches) {
		ctx.fillStyle = hash.hex(String(touch.identifier));
		ctx.beginPath();
		ctx.ellipse(
			touch.clientX,
			touch.clientY,
			touch.radiusX,
			touch.radiusY,
			degreesToRadians(touch.rotationAngle),
			0,
			2 * Math.PI
		);
		ctx.closePath();
		ctx.fill();
	}
});
