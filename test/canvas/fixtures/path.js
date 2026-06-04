// Complex paths with curves
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Heart shape using bezier curves
ctx.fillStyle = '#cc0044';
ctx.beginPath();
ctx.moveTo(100, 170);
ctx.bezierCurveTo(100, 170, 20, 120, 20, 70);
ctx.bezierCurveTo(20, 30, 50, 20, 70, 30);
ctx.bezierCurveTo(85, 37, 100, 55, 100, 55);
ctx.bezierCurveTo(100, 55, 115, 37, 130, 30);
ctx.bezierCurveTo(150, 20, 180, 30, 180, 70);
ctx.bezierCurveTo(180, 120, 100, 170, 100, 170);
ctx.fill();

// Quadratic curve wave
ctx.strokeStyle = '#0066cc';
ctx.lineWidth = 3;
ctx.beginPath();
ctx.moveTo(10, 190);
ctx.quadraticCurveTo(50, 150, 100, 190);
ctx.quadraticCurveTo(150, 230, 190, 190);
ctx.stroke();

// Star shape with straight lines
ctx.fillStyle = 'rgba(255, 200, 0, 0.8)';
ctx.beginPath();
var cx = 100, cy = 40, spikes = 5, outerR = 25, innerR = 10;
for (var i = 0; i < spikes * 2; i++) {
	var r = i % 2 === 0 ? outerR : innerR;
	var angle = (Math.PI * i) / spikes - Math.PI / 2;
	var x = cx + Math.cos(angle) * r;
	var y = cy + Math.sin(angle) * r;
	if (i === 0) ctx.moveTo(x, y);
	else ctx.lineTo(x, y);
}
ctx.closePath();
ctx.fill();
