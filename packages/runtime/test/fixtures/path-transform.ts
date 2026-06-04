import { test } from '../src/tap';

// Path geometry must bake in the current transformation matrix (CTM) at the
// time each segment is added, independent of the CTM in effect when the path
// is later filled / stroked / clipped or hit-tested (HTML Canvas spec). These
// cases build a path under one transform and then change the transform before
// using it — the exact build-time-vs-draw-time CTM distinction that a naive
// "store user-space path, apply CTM at draw" model gets wrong.
//
// Every assertion below is compared against Chrome by the conformance harness,
// so a regression in the path/CTM model is caught automatically.

function ctx(w = 100, h = 50): OffscreenCanvasRenderingContext2D {
	return new OffscreenCanvas(w, h).getContext('2d')!;
}

function alpha(c: OffscreenCanvasRenderingContext2D, x: number, y: number) {
	return c.getImageData(x, y, 1, 1).data[3];
}

// ---------- isPointInPath under a transform changed after building ----------

test('isPointInPath - rect built at identity, then translate', (t) => {
	const c = ctx();
	c.rect(50, 0, 20, 20);
	c.translate(50, 0);
	t.equal(c.isPointInPath(-40, 10), false, 'left of rect');
	t.equal(c.isPointInPath(10, 10), false, 'origin');
	t.equal(c.isPointInPath(49, 10), false, 'just left of rect');
	t.equal(c.isPointInPath(51, 10), true, 'inside left edge');
	t.equal(c.isPointInPath(69, 10), true, 'inside right edge');
	t.equal(c.isPointInPath(71, 10), false, 'just right of rect');
});

test('isPointInPath - built under scale(-1,1)', (t) => {
	const c = ctx();
	c.scale(-1, 1);
	c.rect(-70, 0, 20, 20); // device x in [50,70]
	t.equal(c.isPointInPath(49, 10), false, 'just left');
	t.equal(c.isPointInPath(51, 10), true, 'inside');
	t.equal(c.isPointInPath(69, 10), true, 'inside');
	t.equal(c.isPointInPath(71, 10), false, 'just right');
});

test('isPointInPath - translate, rect, translate', (t) => {
	const c = ctx();
	c.translate(50, 0);
	c.rect(50, 0, 20, 20); // device x in [100,120]
	c.translate(0, 50);
	t.equal(c.isPointInPath(60, 10), false, 'outside');
	t.equal(c.isPointInPath(110, 10), true, 'inside');
	t.equal(c.isPointInPath(110, 60), false, 'below');
});

test('isPointInPath - segments added under different CTMs', (t) => {
	// A triangle whose vertices are each added under a different translation.
	const c = ctx(100, 100);
	c.translate(10, 10);
	c.beginPath();
	c.moveTo(0, 0); // device (10,10)
	c.translate(40, 0);
	c.lineTo(0, 40); // device (50,50)
	c.translate(0, 0);
	c.lineTo(-40, 40); // device (10,50)
	c.closePath();
	t.equal(c.isPointInPath(25, 40), true, 'inside triangle');
	t.equal(c.isPointInPath(5, 5), false, 'outside triangle');
});

// ---------- isPointInStroke: pen is scaled by the stroke-time CTM ----------

test('isPointInStroke - thick line under non-uniform scale', (t) => {
	const c = ctx(200, 200);
	c.lineWidth = 10;
	c.translate(100, 100);
	c.scale(1, 3);
	c.beginPath();
	c.moveTo(-50, 0); // device (50,100)
	c.lineTo(50, 0); // device (150,100)
	// isPointInStroke does NOT apply the current CTM to the query point: the
	// point is in the path's (device) coordinate space, and the pen is scaled
	// by the build-time CTM (lineWidth 10 * yscale 3 => 30px tall in device
	// space, i.e. ±15 around device y=100). Verified against Chrome.
	t.equal(c.isPointInStroke(100, 100), true, 'device centre of line');
	t.equal(c.isPointInStroke(100, 114), true, 'within scaled pen (±15)');
	t.equal(c.isPointInStroke(100, 116), false, 'outside scaled pen');
	t.equal(c.isPointInStroke(0, 0), false, 'current user origin is not on path');
});

// ---------- fill placement honours build-time CTM (pixel checks) ----------

test('fill - rect built under translate, drawn after reset', (t) => {
	const c = ctx(120, 60);
	c.fillStyle = '#ff0000';
	c.save();
	c.translate(60, 10);
	c.beginPath();
	c.rect(0, 0, 40, 30); // device (60,10)..(100,40)
	c.restore();
	c.fill();
	t.ok(alpha(c, 80, 25) > 0, 'filled where built (80,25)');
	t.equal(alpha(c, 10, 25), 0, 'not filled at origin (10,25)');
});

test('fill - rotated square drawn after rotation undone', (t) => {
	const c = ctx(200, 200);
	c.fillStyle = '#00ff00';
	c.save();
	c.translate(100, 100);
	c.rotate(Math.PI / 4);
	c.beginPath();
	c.rect(-20, -20, 40, 40); // a diamond centred at (100,100)
	c.restore();
	c.fill();
	t.ok(alpha(c, 100, 100) > 0, 'centre filled');
	t.ok(alpha(c, 100, 76) > 0, 'top vertex of diamond filled');
	t.equal(alpha(c, 84, 84), 0, 'corner outside diamond is empty');
});

// ---------- clip honours build-time CTM ----------

test('clip - region built under translate, fill after reset', (t) => {
	const c = ctx(200, 200);
	c.save();
	c.translate(40, 140);
	c.beginPath();
	c.rect(0, 0, 50, 50); // clip region device (40,140)..(90,190)
	c.restore();
	c.clip();
	c.fillStyle = '#ff9900';
	c.fillRect(0, 0, 200, 200);
	t.ok(alpha(c, 60, 160) > 0, 'inside clip region painted');
	t.equal(alpha(c, 10, 10), 0, 'outside clip region not painted');
});
