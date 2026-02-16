// Transform operations: translate, rotate, scale
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Translated rectangle
ctx.fillStyle = '#ff0000';
ctx.save();
ctx.translate(50, 50);
ctx.fillRect(0, 0, 40, 40);
ctx.restore();

// Rotated rectangle
ctx.fillStyle = '#00ff00';
ctx.save();
ctx.translate(130, 50);
ctx.rotate(Math.PI / 4);
ctx.fillRect(-20, -20, 40, 40);
ctx.restore();

// Scaled rectangle
ctx.fillStyle = '#0000ff';
ctx.save();
ctx.translate(50, 140);
ctx.scale(2, 0.5);
ctx.fillRect(-15, -15, 30, 30);
ctx.restore();

// Combined transforms
ctx.fillStyle = 'rgba(255, 128, 0, 0.7)';
ctx.save();
ctx.translate(140, 140);
ctx.rotate(Math.PI / 6);
ctx.scale(1.5, 1);
ctx.fillRect(-20, -20, 40, 40);
ctx.restore();
