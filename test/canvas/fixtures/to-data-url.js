// Test toDataURL visual pattern: draw a recognizable quad pattern
// Note: toDataURL/convertToBlob are async and use the thread pool,
// which is not available in the canvas test harness. This fixture
// only tests the visual output; encoding APIs are tested separately.
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Draw a recognizable quad pattern
ctx.fillStyle = '#ff0000';
ctx.fillRect(0, 0, 100, 100);
ctx.fillStyle = '#00ff00';
ctx.fillRect(100, 0, 100, 100);
ctx.fillStyle = '#0000ff';
ctx.fillRect(0, 100, 100, 100);
ctx.fillStyle = '#ffff00';
ctx.fillRect(100, 100, 100, 100);
