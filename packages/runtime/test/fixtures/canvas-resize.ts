import { test } from '../src/tap';

test('OffscreenCanvas resize - context valid after resize (fillRect)', (t) => {
	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;

	// Resize canvas after getting context
	c.width = 200;
	c.height = 50;

	// The context should still be usable after resize
	t.doesNotThrow(() => {
		ctx.fillStyle = 'red';
		ctx.fillRect(0, 0, 200, 50);
	}, 'fillRect should not throw after resize');

	t.equal(c.width, 200, 'canvas width should be 200');
	t.equal(c.height, 50, 'canvas height should be 50');
});

test('OffscreenCanvas resize - context state resets', (t) => {
	const c = new OffscreenCanvas(100, 100);
	const ctx = c.getContext('2d')!;

	// Set some state
	ctx.fillStyle = 'red';
	ctx.globalAlpha = 0.5;

	// Resize should reset state to defaults
	c.width = 200;

	t.equal(ctx.globalAlpha, 1, 'globalAlpha should reset to 1 after resize');
});

test('OffscreenCanvas resize - font resets to default', (t) => {
	const c = new OffscreenCanvas(100, 100);
	const ctx = c.getContext('2d')!;

	// Set a custom font
	ctx.font = '32px sans-serif';
	t.equal(ctx.font, '32px sans-serif', 'font should be 32px sans-serif');

	// Resize should reset font to default
	c.width = 200;

	t.equal(
		ctx.font,
		'10px sans-serif',
		'font should reset to 10px sans-serif after resize',
	);
});

test('OffscreenCanvas resize - getContext returns same object', (t) => {
	const c = new OffscreenCanvas(100, 100);
	const ctx1 = c.getContext('2d')!;
	c.width = 200;
	c.height = 50;
	const ctx2 = c.getContext('2d')!;

	t.equal(ctx1, ctx2, 'getContext should return same context after resize');
});

test('OffscreenCanvas resize - drawing produces correct pixels', (t) => {
	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;

	c.width = 10;
	c.height = 10;

	// Fill entire canvas with red
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 10, 10);

	const imageData = ctx.getImageData(0, 0, 1, 1);
	t.equal(imageData.data[0], 255, 'red channel should be 255');
	t.equal(imageData.data[1], 0, 'green channel should be 0');
	t.equal(imageData.data[2], 0, 'blue channel should be 0');
	t.equal(imageData.data[3], 255, 'alpha channel should be 255');
});

test('OffscreenCanvas resize - canvas clears on resize', (t) => {
	const c = new OffscreenCanvas(10, 10);
	const ctx = c.getContext('2d')!;

	// Fill with red
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 10, 10);

	// Resize (should clear to transparent black)
	c.width = 10;

	const imageData = ctx.getImageData(0, 0, 1, 1);
	t.equal(imageData.data[0], 0, 'red channel should be 0 after clear');
	t.equal(imageData.data[1], 0, 'green channel should be 0 after clear');
	t.equal(imageData.data[2], 0, 'blue channel should be 0 after clear');
	t.equal(imageData.data[3], 0, 'alpha channel should be 0 after clear');
});

test('OffscreenCanvas resize - multiple consecutive resizes', (t) => {
	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;

	c.width = 50;
	c.height = 50;
	c.width = 100;
	c.height = 100;
	c.width = 200;
	c.height = 200;

	t.doesNotThrow(() => {
		ctx.fillStyle = 'blue';
		ctx.fillRect(0, 0, 200, 200);
	}, 'should handle multiple consecutive resizes');

	const imageData = ctx.getImageData(0, 0, 1, 1);
	t.equal(imageData.data[2], 255, 'blue channel should be 255');
	t.equal(imageData.data[3], 255, 'alpha channel should be 255');
});

test('OffscreenCanvas resize - fillText after resize with re-set font', (t) => {
	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;
	c.width = 200;
	c.height = 50;

	// Re-set font after resize, then draw text
	ctx.font = '32px sans-serif';
	t.doesNotThrow(() => {
		ctx.fillText('hello', 0, 40);
	}, 'fillText should work after resize with font re-set');
	t.pass('fillText completed without crash');
});

test('OffscreenCanvas resize - fillText after resize without re-set font (issue #318)', (t) => {
	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;
	ctx.font = '32px sans-serif';

	// Resize WITHOUT re-setting font — font resets to "10px sans-serif"
	c.width = 200;
	c.height = 50;

	// This used to segfault (NULL font pointers after surface reset)
	t.doesNotThrow(() => {
		ctx.fillText('crash', 0, 40);
	}, 'fillText should not crash after resize without re-setting font');
	t.pass('fillText completed without crash');
});

test('OffscreenCanvas resize - strokeText after resize without re-set font', (t) => {
	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;
	ctx.font = '32px sans-serif';

	c.width = 200;
	c.height = 50;

	t.doesNotThrow(() => {
		ctx.strokeText('stroke', 0, 40);
	}, 'strokeText should not crash after resize without re-setting font');
	t.pass('strokeText completed without crash');
});

test('OffscreenCanvas resize - measureText after resize without re-set font', (t) => {
	const c = new OffscreenCanvas(100, 100);
	const ctx = c.getContext('2d')!;
	ctx.font = '32px sans-serif';

	c.width = 200;

	// After resize, font resets to "10px sans-serif" — measureText should
	// still return a positive width (not zero, not crash).
	const m = ctx.measureText('hello');
	t.ok(m.width > 0, 'measureText should return positive width after resize');
});

test('OffscreenCanvas resize - measureText width matches default font', (t) => {
	// Measure with the default "10px sans-serif" on a fresh context
	const ref = new OffscreenCanvas(100, 100);
	const refCtx = ref.getContext('2d')!;
	const refWidth = refCtx.measureText('hello').width;

	// Now create another, set a big font, resize (resets to default), measure
	const c = new OffscreenCanvas(100, 100);
	const ctx = c.getContext('2d')!;
	ctx.font = '48px sans-serif';
	c.width = 200;
	const afterWidth = ctx.measureText('hello').width;

	t.equal(
		afterWidth,
		refWidth,
		'measureText after resize should match default font width',
	);
});

test('OffscreenCanvas resize - Phaser Text pattern', (t) => {
	// This is the exact pattern Phaser 3 uses for Text game objects:
	// 1. Create canvas, get context
	// 2. Set font, measure text
	// 3. Resize canvas to fit
	// 4. Re-set font, draw text

	const c = new OffscreenCanvas(1, 1);
	const ctx = c.getContext('2d')!;

	ctx.font = '32px sans-serif';
	const metrics = ctx.measureText('test text');
	t.ok(metrics.width > 0, 'measureText should return positive width');

	c.width = Math.ceil(metrics.width);
	c.height = 50;

	ctx.font = '32px sans-serif';
	t.doesNotThrow(() => {
		ctx.fillText('test text', 0, 40);
	}, 'fillText should work in Phaser pattern');
	t.pass('Phaser Text pattern completed');
});
