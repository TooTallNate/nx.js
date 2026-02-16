// save/restore state stack
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Set initial state
ctx.fillStyle = '#ff0000';
ctx.lineWidth = 2;
ctx.globalAlpha = 1.0;

// Draw with initial state
ctx.fillRect(10, 10, 40, 40);

// Save and modify
ctx.save();
ctx.fillStyle = '#00ff00';
ctx.lineWidth = 5;
ctx.globalAlpha = 0.5;
ctx.fillRect(60, 10, 40, 40);

// Save again and modify more
ctx.save();
ctx.fillStyle = '#0000ff';
ctx.globalAlpha = 0.3;
ctx.translate(50, 50);
ctx.fillRect(60, 10, 40, 40);

// Restore to second save (green, alpha 0.5)
ctx.restore();
ctx.fillRect(10, 80, 40, 40);

// Restore to initial state (red, alpha 1.0)
ctx.restore();
ctx.fillRect(60, 80, 40, 40);

// Verify line width restored
ctx.strokeStyle = '#000000';
ctx.strokeRect(120, 10, 60, 60);

// Multiple saves/restores shouldn't break
ctx.save();
ctx.save();
ctx.save();
ctx.fillStyle = '#ff00ff';
ctx.fillRect(120, 90, 60, 40);
ctx.restore();
ctx.restore();
ctx.restore();

// After all restores, should be back to original (red)
ctx.fillRect(120, 140, 60, 40);
