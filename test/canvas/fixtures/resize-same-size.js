// Setting same dimensions still clears (canvas.width = canvas.width)
// Per spec, assigning width/height always resets, even to the same value.
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Fill with a checkerboard pattern
ctx.fillStyle = '#ff0000';
ctx.fillRect(0, 0, 100, 100);
ctx.fillStyle = '#00ff00';
ctx.fillRect(100, 0, 100, 100);
ctx.fillStyle = '#0000ff';
ctx.fillRect(0, 100, 100, 100);
ctx.fillStyle = '#ffff00';
ctx.fillRect(100, 100, 100, 100);

// Reset with same dimensions — must clear everything
c.canvas.width = c.canvas.width;

// Draw a single small rect to show it's cleared and working
ctx.fillStyle = '#ff00ff';
ctx.fillRect(75, 75, 50, 50);
