import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Canvas');

const ctx = screen.getContext('2d');

test('`CanvasRenderingContext2D.name`', () => {
	assert.equal(CanvasRenderingContext2D.name, 'CanvasRenderingContext2D');
	assert.equal(
		OffscreenCanvasRenderingContext2D.name,
		'OffscreenCanvasRenderingContext2D'
	);
});

test('`CanvasRenderingContext2D#getImageData()`', () => {
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 1, 1);
	const data = ctx.getImageData(0, 0, 1, 1);
	assert.equal(data.data[0], 255);
	assert.equal(data.data[1], 0);
	assert.equal(data.data[2], 0);
	assert.equal(data.data[3], 255);
});

// 2d.state.saverestore.stackdepth
test('save()/restore() stack depth is not unreasonably limited', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');

	var limit = 512;
	for (var i = 1; i < limit; ++i) {
		ctx.save();
		ctx.lineWidth = i;
	}
	for (var i = limit - 1; i > 0; --i) {
		assert.equal(ctx.lineWidth, i);
		ctx.restore();
	}
});

test('`CanvasRenderingContext2D#isPointInPath()`', () => {
	ctx.resetTransform();
	ctx.beginPath();
	ctx.rect(10, 10, 100, 100);
	assert.equal(ctx.isPointInPath(30, 70), true);
	assert.equal(ctx.isPointInPath(8, 8), false);
});

test('2d.path.isPointInPath.transform.1', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');

	ctx.translate(50, 0);
	ctx.rect(0, 0, 20, 20);
	assert.equal(ctx.isPointInPath(-40, 10), false);
	assert.equal(ctx.isPointInPath(10, 10), false);
	assert.equal(ctx.isPointInPath(49, 10), false);
	assert.equal(ctx.isPointInPath(51, 10), true);
	assert.equal(ctx.isPointInPath(69, 10), true);
	assert.equal(ctx.isPointInPath(71, 10), false);
});

test('2d.path.isPointInPath.transform.2', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');

	ctx.rect(50, 0, 20, 20);
	ctx.translate(50, 0);
	assert.equal(ctx.isPointInPath(-40, 10), false);
	assert.equal(ctx.isPointInPath(10, 10), false);
	assert.equal(ctx.isPointInPath(49, 10), false);
	assert.equal(ctx.isPointInPath(51, 10), true);
	assert.equal(ctx.isPointInPath(69, 10), true);
	assert.equal(ctx.isPointInPath(71, 10), false);
});

test('2d.path.isPointInPath.transform.3', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');

	ctx.scale(-1, 1);
	ctx.rect(-70, 0, 20, 20);
	assert.equal(ctx.isPointInPath(-40, 10), false);
	assert.equal(ctx.isPointInPath(10, 10), false);
	assert.equal(ctx.isPointInPath(49, 10), false);
	assert.equal(ctx.isPointInPath(51, 10), true);
	assert.equal(ctx.isPointInPath(69, 10), true);
	assert.equal(ctx.isPointInPath(71, 10), false);
});

test('2d.path.isPointInPath.transform.3', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');

	ctx.translate(50, 0);
	ctx.rect(50, 0, 20, 20);
	ctx.translate(0, 50);
	assert.equal(ctx.isPointInPath(60, 10), false);
	assert.equal(ctx.isPointInPath(110, 10), true);
	assert.equal(ctx.isPointInPath(110, 60), false);
});

test('`CanvasRenderingContext2D#isPointInStroke()`', () => {
	ctx.lineWidth = 1;
	ctx.resetTransform();
	ctx.beginPath();
	ctx.rect(10, 10, 100, 100);
	assert.equal(ctx.isPointInStroke(50, 10), true);
	assert.equal(ctx.isPointInStroke(30, 70), false);
	assert.equal(ctx.isPointInStroke(8, 8), false);
});

test.run();
