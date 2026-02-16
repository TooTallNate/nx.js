// Arc and circle drawing
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Full circle - filled
ctx.fillStyle = '#ff6600';
ctx.beginPath();
ctx.arc(60, 60, 40, 0, Math.PI * 2);
ctx.fill();

// Half circle - stroked
ctx.strokeStyle = '#0066ff';
ctx.lineWidth = 3;
ctx.beginPath();
ctx.arc(150, 60, 35, 0, Math.PI);
ctx.stroke();

// Quarter arc - filled
ctx.fillStyle = 'rgba(0, 180, 0, 0.6)';
ctx.beginPath();
ctx.moveTo(100, 150);
ctx.arc(100, 150, 40, 0, Math.PI / 2);
ctx.closePath();
ctx.fill();

// Small arc - stroked thick
ctx.strokeStyle = '#cc0000';
ctx.lineWidth = 5;
ctx.beginPath();
ctx.arc(40, 160, 20, Math.PI, Math.PI * 1.5);
ctx.stroke();
