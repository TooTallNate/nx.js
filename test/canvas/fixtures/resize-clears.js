// Resize clears bitmap to transparent black
// After resizing, the canvas should be fully transparent.
// We draw a red rect, resize, then draw a green rect to prove
// the old content was cleared (no red visible).
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Fill entire canvas with red
ctx.fillStyle = '#ff0000';
ctx.fillRect(0, 0, 200, 200);

// Resize the canvas — this must clear the bitmap
c.canvas.width = 200;

// Draw a small green rect to show the canvas is working post-resize
ctx.fillStyle = '#00ff00';
ctx.fillRect(80, 80, 40, 40);
