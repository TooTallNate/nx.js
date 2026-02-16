// setLineDash with various patterns
var c = createCanvas(200, 200);
var ctx = c.ctx;

ctx.strokeStyle = '#333333';
ctx.lineWidth = 2;

// Simple dash
ctx.setLineDash([10, 5]);
ctx.beginPath();
ctx.moveTo(10, 20);
ctx.lineTo(190, 20);
ctx.stroke();

// Longer pattern
ctx.setLineDash([15, 5, 5, 5]);
ctx.beginPath();
ctx.moveTo(10, 50);
ctx.lineTo(190, 50);
ctx.stroke();

// Dotted line
ctx.setLineDash([2, 8]);
ctx.beginPath();
ctx.moveTo(10, 80);
ctx.lineTo(190, 80);
ctx.stroke();

// Dashed circle
ctx.strokeStyle = '#cc0000';
ctx.lineWidth = 3;
ctx.setLineDash([8, 4]);
ctx.beginPath();
ctx.arc(100, 140, 40, 0, Math.PI * 2);
ctx.stroke();

// Dashed rectangle with offset
ctx.strokeStyle = '#0066cc';
ctx.setLineDash([12, 6]);
ctx.lineDashOffset = 6;
ctx.strokeRect(20, 100, 50, 50);

// Reset dash to solid
ctx.setLineDash([]);
ctx.strokeStyle = '#009900';
ctx.lineWidth = 2;
ctx.beginPath();
ctx.moveTo(10, 190);
ctx.lineTo(190, 190);
ctx.stroke();
