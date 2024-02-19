import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Canvas');

const ctx = screen.getContext('2d');

test('`CanvasRenderingContext2D.name`', () => {
	assert.equal(CanvasRenderingContext2D.name, 'CanvasRenderingContext2D');
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

test('`CanvasRenderingContext2D#isPointInPath()`', () => {
	ctx.beginPath();
	ctx.rect(10, 10, 100, 100);
	assert.equal(ctx.isPointInPath(30, 70), true);
	assert.equal(ctx.isPointInPath(8, 8), false);
});

test.run();
