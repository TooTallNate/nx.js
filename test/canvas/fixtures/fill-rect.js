// Basic fillRect with overlapping colors and alpha blending
var c = createCanvas(200, 200);
var ctx = c.ctx;

ctx.fillStyle = '#ff0000';
ctx.fillRect(10, 10, 80, 80);

ctx.fillStyle = '#00ff00';
ctx.fillRect(50, 50, 80, 80);

ctx.fillStyle = 'rgba(0, 0, 255, 0.5)';
ctx.fillRect(90, 30, 80, 80);
