import { test } from '../src/tap';

// Regression: stroking a FULL-circle arc (0 -> 2*PI) must render the ring, not
// collapse to a dot. This reproduces the analog-clock face ring bug where
// ctx.arc(cx, cy, r, 0, 2*PI, true) + stroke drew nothing (SkPath::arcTo cannot
// sweep >= 360 deg in one call, and a naive sweep normalization collapsed 2*PI
// to 0).

function strokeArcCanvas(
	r: number,
	anticlockwise: boolean,
	size = 120,
): CanvasRenderingContext2D {
	const c = new OffscreenCanvas(size, size);
	const ctx = c.getContext('2d')!;
	ctx.strokeStyle = 'blue';
	ctx.lineWidth = 4;
	ctx.beginPath();
	ctx.arc(size / 2, size / 2, r, 0, Math.PI * 2, anticlockwise);
	ctx.stroke();
	return ctx;
}

function opaque(ctx: CanvasRenderingContext2D, x: number, y: number): boolean {
	return ctx.getImageData(x, y, 1, 1).data[3] > 0;
}

test('stroke full-circle arc (anticlockwise) renders the ring', (t) => {
	const r = 40;
	const size = 120;
	const ctx = strokeArcCanvas(r, true, size);
	const cx = size / 2;
	const cy = size / 2;
	// Points ON the ring (within the 4px stroke) at the 4 cardinal angles.
	t.ok(opaque(ctx, cx + r, cy), 'right edge of ring is drawn');
	t.ok(opaque(ctx, cx - r, cy), 'left edge of ring is drawn');
	t.ok(opaque(ctx, cx, cy + r), 'bottom edge of ring is drawn');
	t.ok(opaque(ctx, cx, cy - r), 'top edge of ring is drawn');
	// Center is NOT filled (it's a stroke, not a fill).
	t.notOk(opaque(ctx, cx, cy), 'center of ring is not filled');
});

test('stroke full-circle arc (clockwise) renders the ring', (t) => {
	const r = 40;
	const size = 120;
	const ctx = strokeArcCanvas(r, false, size);
	const cx = size / 2;
	const cy = size / 2;
	t.ok(opaque(ctx, cx + r, cy), 'right edge of ring is drawn');
	t.ok(opaque(ctx, cx, cy - r), 'top edge of ring is drawn');
});

test('fill full-circle arc fills the disc', (t) => {
	const size = 120;
	const c = new OffscreenCanvas(size, size);
	const ctx = c.getContext('2d')!;
	ctx.fillStyle = 'red';
	ctx.beginPath();
	ctx.arc(size / 2, size / 2, 40, 0, Math.PI * 2, true);
	ctx.fill();
	t.ok(
		ctx.getImageData(size / 2, size / 2, 1, 1).data[3] > 0,
		'center of filled disc is opaque',
	);
	t.ok(
		ctx.getImageData(2, 2, 1, 1).data[3] === 0,
		'corner outside disc is transparent',
	);
});
