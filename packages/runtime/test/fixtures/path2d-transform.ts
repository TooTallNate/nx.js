import { test } from '../src/tap';

// Path2D coordinates are in user space and are NOT affected by the canvas
// transform at construction time — the CTM is applied when the path is USED
// (ctx.fill(path2d) / stroke(path2d) / isPointInPath(path2d, ...)). These cases
// verify that a Path2D filled/hit-tested under a transform lands correctly, and
// that addPath() composes its argument's points. Compared against Chrome.

function ctx(w = 100, h = 100): OffscreenCanvasRenderingContext2D {
	return new OffscreenCanvas(w, h).getContext('2d')!;
}
function alpha(c: OffscreenCanvasRenderingContext2D, x: number, y: number) {
	return c.getImageData(x, y, 1, 1).data[3];
}

test('Path2D - fill under translate applies CTM at fill time', (t) => {
	const c = ctx(120, 120);
	const p = new Path2D();
	p.rect(0, 0, 30, 30); // user-space rect at origin
	c.fillStyle = '#f00';
	c.translate(50, 40);
	c.fill(p); // should paint device (50,40)..(80,70)
	t.ok(alpha(c, 60, 50) > 0, 'painted at translated position');
	t.equal(alpha(c, 10, 10), 0, 'not painted at user origin');
});

test('Path2D - isPointInPath bakes CTM into path, not the query point', (t) => {
	const c = ctx();
	const p = new Path2D();
	p.rect(0, 0, 20, 20);
	c.translate(50, 50); // path baked to device (50,50)..(70,70)
	// The query point is NOT re-transformed by the CTM (verified vs Chrome);
	// it is tested against the device-space path directly.
	t.equal(c.isPointInPath(p, 55, 55), true, 'device point inside');
	t.equal(c.isPointInPath(p, 5, 5), false, 'user-origin point outside');
	t.equal(c.isPointInPath(p, -5, -5), false, 'outside');
});

test('Path2D - addPath composes another path', (t) => {
	const a = new Path2D();
	a.rect(0, 0, 20, 20);
	const b = new Path2D();
	b.rect(40, 0, 20, 20);
	a.addPath(b);
	const c = ctx();
	c.fillStyle = '#0f0';
	c.fill(a);
	t.ok(alpha(c, 10, 10) > 0, 'first rect filled');
	t.ok(alpha(c, 50, 10) > 0, 'added rect filled');
	t.equal(alpha(c, 30, 10), 0, 'gap between not filled');
});

test('Path2D - addPath with a transform matrix', (t) => {
	const a = new Path2D();
	const b = new Path2D();
	b.rect(0, 0, 20, 20);
	// Add b translated by (40,0) via a DOMMatrix.
	a.addPath(b, new DOMMatrix().translate(40, 0));
	const c = ctx();
	c.fillStyle = '#00f';
	c.fill(a);
	t.ok(alpha(c, 50, 10) > 0, 'transformed added rect filled at +40');
	t.equal(alpha(c, 10, 10), 0, 'original position empty');
});

test('Path2D - SVG constructor fills correctly under scale', (t) => {
	const p = new Path2D('M0 0 L20 0 L20 20 L0 20 Z');
	const c = ctx();
	c.fillStyle = '#f0f';
	c.scale(2, 2);
	c.fill(p); // device rect (0,0)..(40,40)
	t.ok(alpha(c, 30, 30) > 0, 'scaled SVG path covers device (30,30)');
	t.ok(alpha(c, 10, 10) > 0, 'covers device (10,10)');
});
