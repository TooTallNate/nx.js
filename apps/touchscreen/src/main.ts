import ColorHash from 'color-hash';

const hash = new ColorHash();
const ctx = Switch.screen.getContext('2d');

Switch.addEventListener('touchmove', (e) => {
	for (const touch of e.changedTouches) {
		ctx.fillStyle = hash.hex(String(touch.identifier));
		ctx.fillRect(
			touch.screenX - touch.radiusX,
			touch.screenY - touch.radiusY,
			touch.radiusX * 2,
			touch.radiusY * 2
		);
	}
});
