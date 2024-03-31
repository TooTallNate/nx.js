import ColorHash from 'color-hash';

const hash = new ColorHash();
const ctx = screen.getContext('2d');

ctx.fillStyle = 'white';
ctx.font = '48px system-ui';
ctx.fillText('Touch the screen to draw...', 320, 350);

screen.addEventListener(
	'touchstart',
	(e) => {
		ctx.clearRect(0, 0, screen.width, screen.height);
	},
	{ once: true },
);

screen.addEventListener('touchmove', (e) => {
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
			2 * Math.PI,
		);
		ctx.closePath();
		ctx.fill();
	}
});

function degreesToRadians(degrees: number) {
	return degrees * (Math.PI / 180);
}
