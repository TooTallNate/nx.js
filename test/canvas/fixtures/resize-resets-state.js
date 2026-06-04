// Resize resets context state (fillStyle, transform, globalAlpha)
// After resizing, all context state must return to defaults.
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Set non-default state
ctx.fillStyle = '#ff0000';
ctx.globalAlpha = 0.5;
ctx.translate(50, 50);
ctx.scale(2, 2);
ctx.lineWidth = 10;

// Resize — must reset all state to defaults
c.canvas.height = 200;

// Draw with default state (fillStyle should be black, globalAlpha 1,
// transform identity, lineWidth 1)
ctx.fillStyle = '#0000ff';
ctx.fillRect(10, 10, 80, 80);

// Stroke with default lineWidth (should be 1)
ctx.strokeStyle = '#ff0000';
ctx.strokeRect(100, 10, 80, 80);
