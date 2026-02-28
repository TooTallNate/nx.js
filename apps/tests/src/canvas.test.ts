import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Canvas');

const ctx = screen.getContext('2d');

test('`CanvasRenderingContext2D.name`', () => {
	assert.equal(CanvasRenderingContext2D.name, 'CanvasRenderingContext2D');
	assert.equal(
		OffscreenCanvasRenderingContext2D.name,
		'OffscreenCanvasRenderingContext2D',
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

// Canvas resize conformance tests
// Per HTML spec, setting width or height must:
// 1. Update the dimension
// 2. Clear the bitmap to transparent black
// 3. Reset the context to its default state

test('setting width updates the canvas dimension', () => {
	var canvas = new OffscreenCanvas(100, 50);
	assert.equal(canvas.width, 100);
	canvas.width = 200;
	assert.equal(canvas.width, 200);
});

test('setting height updates the canvas dimension', () => {
	var canvas = new OffscreenCanvas(100, 50);
	assert.equal(canvas.height, 50);
	canvas.height = 150;
	assert.equal(canvas.height, 150);
});

test('setting width clears the canvas to transparent black', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 100, 50);
	canvas.width = 100; // same value still clears
	var data = ctx.getImageData(0, 0, 1, 1);
	assert.equal(data.data[0], 0);
	assert.equal(data.data[1], 0);
	assert.equal(data.data[2], 0);
	assert.equal(data.data[3], 0);
});

test('setting height clears the canvas to transparent black', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 100, 50);
	canvas.height = 50; // same value still clears
	var data = ctx.getImageData(0, 0, 1, 1);
	assert.equal(data.data[0], 0);
	assert.equal(data.data[1], 0);
	assert.equal(data.data[2], 0);
	assert.equal(data.data[3], 0);
});

test('setting width resets fillStyle to black', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.fillStyle = 'red';
	canvas.width = 200;
	assert.equal(ctx.fillStyle, '#000000');
});

test('setting width resets strokeStyle to black', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.strokeStyle = 'blue';
	canvas.width = 200;
	assert.equal(ctx.strokeStyle, '#000000');
});

test('setting width resets lineWidth to 1', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.lineWidth = 5;
	canvas.width = 200;
	assert.equal(ctx.lineWidth, 1);
});

test('setting width resets globalAlpha to 1', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.globalAlpha = 0.5;
	canvas.width = 200;
	assert.equal(ctx.globalAlpha, 1);
});

test('setting width resets transform to identity', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.translate(50, 50);
	ctx.scale(2, 2);
	canvas.width = 200;
	// After reset, the transform should be identity.
	// Drawing at (0,0) should place pixels at (0,0).
	ctx.fillStyle = 'red';
	ctx.fillRect(0, 0, 1, 1);
	var data = ctx.getImageData(0, 0, 1, 1);
	assert.equal(data.data[0], 255);
	assert.equal(data.data[3], 255);
});

test('setting width resets the current path', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.rect(0, 0, 100, 50);
	assert.equal(ctx.isPointInPath(50, 25), true);
	canvas.width = 200;
	// Path should be cleared after resize
	assert.equal(ctx.isPointInPath(50, 25), false);
});

test('setting width clears the save/restore stack', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.lineWidth = 5;
	ctx.save();
	ctx.lineWidth = 10;
	canvas.width = 200;
	// After resize, state stack is empty, lineWidth is reset to 1
	assert.equal(ctx.lineWidth, 1);
	// restore() should be a no-op (stack is empty)
	ctx.restore();
	assert.equal(ctx.lineWidth, 1);
});

test('setting width resets imageSmoothingEnabled to true', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.imageSmoothingEnabled = false;
	canvas.width = 200;
	assert.equal(ctx.imageSmoothingEnabled, true);
});

test('setting width resets textAlign to start', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.textAlign = 'center';
	canvas.width = 200;
	assert.equal(ctx.textAlign, 'start');
});

test('setting width resets textBaseline to alphabetic', () => {
	var canvas = new OffscreenCanvas(100, 50);
	var ctx = canvas.getContext('2d');
	ctx.textBaseline = 'top';
	canvas.width = 200;
	assert.equal(ctx.textBaseline, 'alphabetic');
});

test('drawing works correctly after resize', () => {
	var canvas = new OffscreenCanvas(50, 50);
	var ctx = canvas.getContext('2d');
	canvas.width = 100;
	canvas.height = 100;
	ctx.fillStyle = 'green';
	ctx.fillRect(50, 50, 1, 1);
	var data = ctx.getImageData(50, 50, 1, 1);
	assert.equal(data.data[0], 0);
	assert.equal(data.data[1], 128); // green
	assert.equal(data.data[2], 0);
	assert.equal(data.data[3], 255);
});

// convertToBlob tests
test('`OffscreenCanvas#convertToBlob()` returns a Blob', async () => {
	var canvas = new OffscreenCanvas(10, 10);
	var ctx = canvas.getContext('2d');
	ctx.fillStyle = 'green';
	ctx.fillRect(0, 0, 10, 10);
	var blob = await canvas.convertToBlob();
	assert.ok(blob instanceof Blob, 'should return a Blob');
	assert.equal(blob.type, 'image/png');
	assert.ok(blob.size > 0, 'blob should have data');
});

test('`OffscreenCanvas#convertToBlob()` with JPEG type', async () => {
	var canvas = new OffscreenCanvas(10, 10);
	var ctx = canvas.getContext('2d');
	ctx.fillStyle = 'yellow';
	ctx.fillRect(0, 0, 10, 10);
	var blob = await canvas.convertToBlob({ type: 'image/jpeg' });
	assert.ok(blob instanceof Blob, 'should return a Blob');
	assert.equal(blob.type, 'image/jpeg');
	assert.ok(blob.size > 0, 'blob should have data');
});

test('`OffscreenCanvas#convertToBlob()` data is valid PNG', async () => {
	var canvas = new OffscreenCanvas(2, 2);
	var ctx = canvas.getContext('2d');
	ctx.fillStyle = '#ff00ff';
	ctx.fillRect(0, 0, 2, 2);
	var blob = await canvas.convertToBlob();
	var buf = new Uint8Array(await blob.arrayBuffer());
	// PNG magic bytes: 137 80 78 71 13 10 26 10
	assert.equal(buf[0], 137);
	assert.equal(buf[1], 80); // P
	assert.equal(buf[2], 78); // N
	assert.equal(buf[3], 71); // G
});

// screen.toDataURL tests
test('`screen.toDataURL()` returns a PNG data URL', () => {
	var url = screen.toDataURL();
	assert.ok(
		url.startsWith('data:image/png;base64,'),
		'should start with PNG data URL prefix',
	);
});

// screen.toBlob tests
test('`screen.toBlob()` calls back with a Blob', async () => {
	var blob: Blob | null = await new Promise((resolve) => {
		screen.toBlob((b) => resolve(b));
	});
	assert.ok(blob instanceof Blob, 'should return a Blob');
	assert.equal(blob!.type, 'image/png');
	assert.ok(blob!.size > 0, 'blob should have data');
});

test.run();
