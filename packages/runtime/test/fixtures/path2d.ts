import { test } from '../src/tap';

// Helper: create a canvas, fill a Path2D, return pixel at (x, y)
function fillAndSample(
	path: Path2D,
	x: number,
	y: number,
	size = 50,
): Uint8ClampedArray {
	const c = new OffscreenCanvas(size, size);
	const ctx = c.getContext('2d')!;
	ctx.fillStyle = 'red';
	ctx.fill(path);
	return ctx.getImageData(x, y, 1, 1).data;
}

function isOpaque(pixel: Uint8ClampedArray): boolean {
	return pixel[3] > 0;
}

function isTransparent(pixel: Uint8ClampedArray): boolean {
	return pixel[3] === 0;
}

// ---------- Construction ----------

test('Path2D constructor - empty', (t) => {
	const p = new Path2D();
	t.ok(p instanceof Path2D, 'should be instance of Path2D');
	// An empty path drawn on canvas should produce no pixels
	const pixel = fillAndSample(p, 25, 25);
	t.ok(isTransparent(pixel), 'empty path should not fill any pixels');
});

test('Path2D constructor - from SVG string', (t) => {
	// A rect covering the whole 50x50 canvas
	const p = new Path2D('M 0 0 L 50 0 L 50 50 L 0 50 Z');
	const pixel = fillAndSample(p, 25, 25);
	t.ok(isOpaque(pixel), 'SVG path should fill center pixel');
});

test('Path2D constructor - from another Path2D (copy)', (t) => {
	const original = new Path2D();
	original.rect(0, 0, 50, 50);

	const copy = new Path2D(original);
	const pixel = fillAndSample(copy, 25, 25);
	t.ok(isOpaque(pixel), 'copied path should fill center pixel');

	// Modifying the original should not affect the copy
	original.rect(0, 0, 0, 0); // no-op rect, but mutates original's commands
	const pixel2 = fillAndSample(copy, 25, 25);
	t.ok(isOpaque(pixel2), 'copy is independent of original');
});

// ---------- addPath ----------

test('Path2D addPath - combines two paths', (t) => {
	const p1 = new Path2D();
	p1.rect(0, 0, 25, 50); // left half

	const p2 = new Path2D();
	p2.rect(25, 0, 25, 50); // right half

	p1.addPath(p2);

	// Both halves should be filled
	const left = fillAndSample(p1, 12, 25);
	const right = fillAndSample(p1, 37, 25);
	t.ok(isOpaque(left), 'left half should be filled');
	t.ok(isOpaque(right), 'right half should be filled after addPath');
});

// ---------- rect ----------

test('Path2D rect - fills rectangle area', (t) => {
	const p = new Path2D();
	p.rect(10, 10, 30, 30);

	const inside = fillAndSample(p, 25, 25);
	const outside = fillAndSample(p, 5, 5);
	t.ok(isOpaque(inside), 'pixel inside rect should be filled');
	t.ok(isTransparent(outside), 'pixel outside rect should be transparent');
});

// ---------- moveTo / lineTo / closePath ----------

test('Path2D moveTo/lineTo/closePath - triangle', (t) => {
	const p = new Path2D();
	p.moveTo(25, 5);
	p.lineTo(45, 45);
	p.lineTo(5, 45);
	p.closePath();

	const inside = fillAndSample(p, 25, 30);
	const outside = fillAndSample(p, 5, 5);
	t.ok(isOpaque(inside), 'pixel inside triangle should be filled');
	t.ok(isTransparent(outside), 'pixel outside triangle should be transparent');
});

// ---------- arc ----------

test('Path2D arc - full circle', (t) => {
	const p = new Path2D();
	p.arc(25, 25, 20, 0, Math.PI * 2, false);

	const center = fillAndSample(p, 25, 25);
	const corner = fillAndSample(p, 0, 0);
	t.ok(isOpaque(center), 'center of circle should be filled');
	t.ok(isTransparent(corner), 'corner outside circle should be transparent');
});

// ---------- ellipse ----------

test('Path2D ellipse - wide ellipse', (t) => {
	const p = new Path2D();
	// Wide ellipse: rx=24, ry=10
	p.ellipse(25, 25, 24, 10, 0, 0, Math.PI * 2, false);

	const center = fillAndSample(p, 25, 25);
	const above = fillAndSample(p, 25, 5);
	t.ok(isOpaque(center), 'center of ellipse should be filled');
	t.ok(isTransparent(above), 'well above ellipse should be transparent');
});

// ---------- bezierCurveTo ----------

test('Path2D bezierCurveTo - curve fills expected region', (t) => {
	const p = new Path2D();
	p.moveTo(0, 50);
	p.bezierCurveTo(0, 0, 50, 0, 50, 50);
	p.closePath();

	// The area under the curve should be filled
	const inside = fillAndSample(p, 25, 40);
	t.ok(isOpaque(inside), 'area under bezier curve should be filled');
});

// ---------- quadraticCurveTo ----------

test('Path2D quadraticCurveTo - curve fills expected region', (t) => {
	const p = new Path2D();
	p.moveTo(0, 50);
	p.quadraticCurveTo(25, 0, 50, 50);
	p.closePath();

	const inside = fillAndSample(p, 25, 40);
	t.ok(isOpaque(inside), 'area under quadratic curve should be filled');
});

// ---------- roundRect ----------

test('Path2D roundRect - fills rounded rectangle', (t) => {
	const p = new Path2D();
	p.roundRect(5, 5, 40, 40, 5);

	const center = fillAndSample(p, 25, 25);
	const corner = fillAndSample(p, 0, 0);
	t.ok(isOpaque(center), 'center of roundRect should be filled');
	t.ok(isTransparent(corner), 'corner outside roundRect should be transparent');
});

// ---------- SVG path parsing ----------

test('Path2D SVG - relative commands (lowercase)', (t) => {
	// Relative rect: move to 5,5 then draw 40x40 box
	const p = new Path2D('M 5 5 l 40 0 l 0 40 l -40 0 z');
	const inside = fillAndSample(p, 25, 25);
	const outside = fillAndSample(p, 2, 2);
	t.ok(isOpaque(inside), 'relative SVG path should fill center');
	t.ok(
		isTransparent(outside),
		'outside relative SVG path should be transparent',
	);
});

test('Path2D SVG - horizontal and vertical lines', (t) => {
	const p = new Path2D('M 5 5 H 45 V 45 H 5 Z');
	const inside = fillAndSample(p, 25, 25);
	t.ok(isOpaque(inside), 'H/V commands should draw rect');
});

test('Path2D SVG - cubic bezier (C command)', (t) => {
	const p = new Path2D('M 0 50 C 0 0 50 0 50 50 Z');
	const inside = fillAndSample(p, 25, 40);
	t.ok(isOpaque(inside), 'SVG cubic bezier should fill area');
});

test('Path2D SVG - quadratic bezier (Q command)', (t) => {
	const p = new Path2D('M 0 50 Q 25 0 50 50 Z');
	const inside = fillAndSample(p, 25, 40);
	t.ok(isOpaque(inside), 'SVG quadratic bezier should fill area');
});

// ---------- ctx.stroke(path) ----------

test('Path2D with ctx.stroke - line is rendered', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;

	const p = new Path2D();
	p.moveTo(0, 25);
	p.lineTo(50, 25);

	ctx.strokeStyle = 'blue';
	ctx.lineWidth = 4;
	ctx.stroke(p);

	const onLine = ctx.getImageData(25, 25, 1, 1).data;
	const offLine = ctx.getImageData(25, 0, 1, 1).data;
	t.ok(onLine[3] > 0, 'pixel on stroke line should be visible');
	t.ok(offLine[3] === 0, 'pixel off stroke line should be transparent');
});

// ---------- arcTo ----------

test('Path2D arcTo - rounded corner', (t) => {
	const p = new Path2D();
	p.moveTo(10, 45);
	p.lineTo(10, 10);
	p.arcTo(10, 0, 25, 0, 10);
	p.lineTo(45, 0);
	p.lineTo(45, 45);
	p.closePath();

	const inside = fillAndSample(p, 25, 20);
	t.ok(isOpaque(inside), 'arcTo path should fill inner area');
});

// ---------- Multiple fills with different paths ----------

test('Path2D - two separate paths rendered independently', (t) => {
	const c = new OffscreenCanvas(100, 50);
	const ctx = c.getContext('2d')!;

	const p1 = new Path2D();
	p1.rect(5, 5, 40, 40);

	const p2 = new Path2D();
	p2.rect(55, 5, 40, 40);

	ctx.fillStyle = 'red';
	ctx.fill(p1);
	ctx.fillStyle = 'blue';
	ctx.fill(p2);

	const left = ctx.getImageData(25, 25, 1, 1).data;
	const right = ctx.getImageData(75, 25, 1, 1).data;
	const gap = ctx.getImageData(50, 25, 1, 1).data;

	t.ok(left[0] === 255 && left[2] === 0, 'left rect should be red');
	t.ok(right[2] === 255 && right[0] === 0, 'right rect should be blue');
	t.ok(gap[3] === 0, 'gap between rects should be transparent');
});

// ---------- clip(Path2D) ----------
// Regression test for #323: ctx.clip(path) with a Path2D argument was ignored
// (only the context's current path was clipped against), so drawing outside the
// Path2D region was NOT clipped.

test('clip(Path2D) - restricts subsequent drawing to the path', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;

	const clipPath = new Path2D();
	clipPath.rect(10, 10, 30, 30);

	ctx.clip(clipPath);
	ctx.fillStyle = 'green';
	ctx.fillRect(0, 0, 50, 50); // attempt to fill the entire canvas

	const inside = ctx.getImageData(25, 25, 1, 1).data;
	const outside = ctx.getImageData(5, 5, 1, 1).data;

	t.equal(inside[3], 255, 'inside the clip path should be painted');
	t.equal(outside[3], 0, 'outside the clip path should be clipped out');
});

test('clip(Path2D) - does not disturb the context current path', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;

	// Build a current path that is DIFFERENT from the clip path.
	ctx.rect(0, 0, 20, 20);

	const clipPath = new Path2D();
	clipPath.rect(10, 10, 30, 30);
	ctx.clip(clipPath);

	// fill() with no arg should use the context's current path (0,0,20,20),
	// intersected with the clip region (10,10,30,30) -> (10,10)..(20,20).
	ctx.fillStyle = 'green';
	ctx.fill();

	const overlap = ctx.getImageData(15, 15, 1, 1).data;
	const onlyCurrentPath = ctx.getImageData(5, 5, 1, 1).data; // in path, out of clip
	const onlyClip = ctx.getImageData(30, 30, 1, 1).data; // in clip, out of path

	t.equal(overlap[3], 255, 'current-path ∩ clip is painted');
	t.equal(onlyCurrentPath[3], 0, 'current path outside clip is clipped');
	t.equal(onlyClip[3], 0, 'clip region outside current path is not painted');
});

test('clip(Path2D) - honors the current transform', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;

	// The Path2D coordinates are interpreted under the CTM at clip() time.
	ctx.translate(10, 10);
	const clipPath = new Path2D();
	clipPath.rect(0, 0, 30, 30); // device space (10,10)..(40,40)
	ctx.clip(clipPath);

	ctx.resetTransform();
	ctx.fillStyle = 'green';
	ctx.fillRect(0, 0, 50, 50);

	const inside = ctx.getImageData(25, 25, 1, 1).data;
	const outside = ctx.getImageData(5, 5, 1, 1).data;

	t.equal(inside[3], 255, 'inside the transformed clip path is painted');
	t.equal(outside[3], 0, 'outside the transformed clip path is clipped');
});

test('clip(Path2D, "evenodd") - applies the fill rule to the clip path', (t) => {
	const c = new OffscreenCanvas(60, 60);
	const ctx = c.getContext('2d')!;

	// Outer rect with an inner rect; under evenodd the inner region is a hole.
	const clipPath = new Path2D();
	clipPath.rect(10, 10, 40, 40);
	clipPath.rect(20, 20, 20, 20);
	ctx.clip(clipPath, 'evenodd');

	ctx.fillStyle = 'green';
	ctx.fillRect(0, 0, 60, 60);

	const ring = ctx.getImageData(15, 15, 1, 1).data; // between outer and inner
	const hole = ctx.getImageData(30, 30, 1, 1).data; // inside inner (the hole)
	const outside = ctx.getImageData(5, 5, 1, 1).data;

	t.equal(ring[3], 255, 'the evenodd ring is painted');
	t.equal(hole[3], 0, 'the evenodd hole is clipped out');
	t.equal(outside[3], 0, 'outside the clip path is clipped out');
});

// clip() overload resolution + argument validation (matches Chrome/WebIDL).

test('clip(fillRule) - string arg clips the current path, does not throw', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;

	// Outer + inner rect on the CURRENT path; evenodd makes the inner a hole.
	ctx.rect(10, 10, 30, 30);
	ctx.rect(20, 20, 10, 10);
	ctx.clip('evenodd');

	ctx.fillStyle = 'green';
	ctx.fillRect(0, 0, 50, 50);

	const ring = ctx.getImageData(12, 12, 1, 1).data;
	const hole = ctx.getImageData(25, 25, 1, 1).data;
	const outside = ctx.getImageData(2, 2, 1, 1).data;

	t.equal(ring[3], 255, 'evenodd ring of the current path is painted');
	t.equal(hole[3], 0, 'evenodd hole is clipped out');
	t.equal(outside[3], 0, 'outside the current path is clipped out');
});

test('clip/fill/stroke - non-Path2D object throws (not a silent empty path)', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;
	// A non-Path2D object where a Path2D is expected is a TypeError — matching
	// Chrome — rather than silently producing an empty path (which for clip()
	// would erase all subsequent drawing).
	t.throws(
		// @ts-expect-error - intentionally passing a non-Path2D object
		() => ctx.clip({}),
		undefined,
		'clip(non-Path2D object) throws',
	);
	t.throws(
		// @ts-expect-error - intentionally passing a non-Path2D object
		() => ctx.fill({}),
		undefined,
		'fill(non-Path2D object) throws',
	);
	t.throws(
		// @ts-expect-error - intentionally passing a non-Path2D object
		() => ctx.stroke({}),
		undefined,
		'stroke(non-Path2D object) throws',
	);
});

test('clip/fill - two args with a string path arg throws (Path2D overload)', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;
	// With 2 arguments the (Path2D, fillRule) overload is selected, so a string
	// in the path position is a TypeError (matching Chrome) — NOT treated as a
	// fill rule with the extra arg ignored.
	t.throws(
		// @ts-expect-error - intentionally passing a string where Path2D expected
		() => ctx.clip('nonzero', 123),
		undefined,
		'clip(string, extra) throws (selects the Path2D overload)',
	);
	t.throws(
		// @ts-expect-error - intentionally passing a string where Path2D expected
		() => ctx.fill('nonzero', 123),
		undefined,
		'fill(string, extra) throws (selects the Path2D overload)',
	);
});

test('clip/fill/stroke - trailing args after a Path2D are ignored', (t) => {
	const c = new OffscreenCanvas(50, 50);
	const ctx = c.getContext('2d')!;
	const p = new Path2D();
	p.rect(10, 10, 20, 20);
	t.doesNotThrow(
		// @ts-expect-error - intentionally passing an extra trailing argument
		() => ctx.clip(p, 'evenodd', 9),
		'clip(Path2D, fillRule, extra) ignores the trailing arg',
	);
	t.doesNotThrow(
		// @ts-expect-error - intentionally passing an extra trailing argument
		() => ctx.fill(p, 'evenodd', 9),
		'fill(Path2D, fillRule, extra) ignores the trailing arg',
	);
	t.doesNotThrow(
		// @ts-expect-error - intentionally passing an extra trailing argument
		() => ctx.stroke(p, 9),
		'stroke(Path2D, extra) ignores the trailing arg',
	);
});
