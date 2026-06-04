// Linear gradients: horizontal, vertical, diagonal, multi-stop
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Horizontal gradient
var g1 = ctx.createLinearGradient(0, 0, 100, 0);
g1.addColorStop(0, 'red');
g1.addColorStop(1, 'blue');
ctx.fillStyle = g1;
ctx.fillRect(0, 0, 100, 80);

// Vertical gradient
var g2 = ctx.createLinearGradient(0, 0, 0, 80);
g2.addColorStop(0, '#00ff00');
g2.addColorStop(1, '#000000');
ctx.fillStyle = g2;
ctx.fillRect(100, 0, 100, 80);

// Diagonal gradient with multiple stops
var g3 = ctx.createLinearGradient(0, 100, 200, 200);
g3.addColorStop(0, 'yellow');
g3.addColorStop(0.5, 'purple');
g3.addColorStop(1, 'cyan');
ctx.fillStyle = g3;
ctx.fillRect(0, 100, 200, 100);

// Stroke with gradient
var g4 = ctx.createLinearGradient(0, 80, 100, 80);
g4.addColorStop(0, 'orange');
g4.addColorStop(1, 'white');
ctx.strokeStyle = g4;
ctx.lineWidth = 4;
ctx.strokeRect(5, 85, 90, 10);
