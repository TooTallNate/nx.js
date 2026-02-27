// Test toDataURL - draws a red rectangle, then verifies toDataURL produces a valid data URL
var c = createCanvas(200, 200);
var ctx = c.ctx;

ctx.fillStyle = '#ff0000';
ctx.fillRect(50, 50, 100, 100);
