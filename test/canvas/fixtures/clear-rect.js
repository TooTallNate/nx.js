// clearRect behavior
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Fill entire canvas
ctx.fillStyle = '#3366cc';
ctx.fillRect(0, 0, 200, 200);

// Clear a rectangle in the center
ctx.clearRect(50, 50, 100, 100);

// Draw over the cleared area partially
ctx.fillStyle = 'rgba(255, 0, 0, 0.5)';
ctx.fillRect(70, 70, 60, 60);

// Clear a small corner
ctx.clearRect(0, 0, 30, 30);

// Clear overlapping the edge
ctx.clearRect(170, 170, 50, 50);
