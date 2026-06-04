// Ellipse drawing
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Basic ellipse - filled
ctx.fillStyle = '#3366cc';
ctx.beginPath();
ctx.ellipse(70, 60, 50, 30, 0, 0, Math.PI * 2);
ctx.fill();

// Rotated ellipse - filled
ctx.fillStyle = 'rgba(204, 51, 51, 0.7)';
ctx.beginPath();
ctx.ellipse(140, 60, 40, 20, Math.PI / 4, 0, Math.PI * 2);
ctx.fill();

// Stroked ellipse
ctx.strokeStyle = '#009933';
ctx.lineWidth = 3;
ctx.beginPath();
ctx.ellipse(70, 150, 50, 25, 0, 0, Math.PI * 2);
ctx.stroke();

// Partial ellipse arc
ctx.fillStyle = '#ff9900';
ctx.beginPath();
ctx.moveTo(150, 150);
ctx.ellipse(150, 150, 35, 20, 0, 0, Math.PI * 1.2);
ctx.closePath();
ctx.fill();
