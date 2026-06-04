// Radial gradients: basic, offset center, concentric rings
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Basic radial gradient (centered)
var g1 = ctx.createRadialGradient(50, 50, 5, 50, 50, 50);
g1.addColorStop(0, 'white');
g1.addColorStop(1, 'red');
ctx.fillStyle = g1;
ctx.fillRect(0, 0, 100, 100);

// Offset inner circle (light source effect)
var g2 = ctx.createRadialGradient(130, 30, 5, 150, 50, 50);
g2.addColorStop(0, 'white');
g2.addColorStop(0.5, '#0066ff');
g2.addColorStop(1, '#000033');
ctx.fillStyle = g2;
ctx.fillRect(100, 0, 100, 100);

// Large radial gradient filling bottom half
var g3 = ctx.createRadialGradient(100, 150, 10, 100, 150, 80);
g3.addColorStop(0, '#ffff00');
g3.addColorStop(0.4, '#ff6600');
g3.addColorStop(0.8, '#cc0000');
g3.addColorStop(1, '#330000');
ctx.fillStyle = g3;
ctx.fillRect(0, 100, 200, 100);

// Stroke with radial gradient
var g4 = ctx.createRadialGradient(100, 150, 0, 100, 150, 80);
g4.addColorStop(0, 'lime');
g4.addColorStop(1, 'transparent');
ctx.strokeStyle = g4;
ctx.lineWidth = 3;
ctx.beginPath();
ctx.arc(100, 150, 40, 0, Math.PI * 2);
ctx.stroke();
