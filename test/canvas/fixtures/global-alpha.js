// globalAlpha blending
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Draw background grid of colored rectangles
var colors = ['#ff0000', '#00ff00', '#0000ff', '#ffff00'];
for (var i = 0; i < 4; i++) {
	ctx.fillStyle = colors[i];
	ctx.fillRect((i % 2) * 100, Math.floor(i / 2) * 100, 100, 100);
}

// Draw semi-transparent overlapping circles
ctx.globalAlpha = 0.5;
ctx.fillStyle = '#ffffff';
ctx.beginPath();
ctx.arc(70, 70, 50, 0, Math.PI * 2);
ctx.fill();

ctx.globalAlpha = 0.3;
ctx.fillStyle = '#000000';
ctx.beginPath();
ctx.arc(130, 130, 50, 0, Math.PI * 2);
ctx.fill();

// Very faint overlay
ctx.globalAlpha = 0.1;
ctx.fillStyle = '#ff00ff';
ctx.fillRect(0, 0, 200, 200);

// Reset and draw solid
ctx.globalAlpha = 1.0;
ctx.fillStyle = '#000000';
ctx.fillRect(90, 90, 20, 20);
