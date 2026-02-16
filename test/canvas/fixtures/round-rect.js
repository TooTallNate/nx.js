// roundRect with various corner radii
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Uniform radius
ctx.fillStyle = '#3366cc';
ctx.beginPath();
ctx.roundRect(10, 10, 80, 60, 10);
ctx.fill();

// Larger radius
ctx.fillStyle = '#cc6633';
ctx.beginPath();
ctx.roundRect(100, 10, 80, 60, 20);
ctx.fill();

// Stroked round rect
ctx.strokeStyle = '#339933';
ctx.lineWidth = 3;
ctx.beginPath();
ctx.roundRect(10, 90, 80, 60, 15);
ctx.stroke();

// Array of radii [topLeft, topRight, bottomRight, bottomLeft]
ctx.fillStyle = '#993399';
ctx.beginPath();
ctx.roundRect(100, 90, 80, 60, [5, 15, 25, 35]);
ctx.fill();

// Pill shape (radius > half height)
ctx.fillStyle = '#ff9900';
ctx.beginPath();
ctx.roundRect(30, 170, 140, 20, 10);
ctx.fill();
