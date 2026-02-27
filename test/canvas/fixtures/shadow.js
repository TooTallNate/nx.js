// Shadow support: shadowColor, shadowOffsetX, shadowOffsetY
var c = createCanvas(200, 200);
var ctx = c.ctx;

// White background so shadows are visible
ctx.fillStyle = 'white';
ctx.fillRect(0, 0, 200, 200);

// 1. Black shadow offset - fillRect
ctx.shadowColor = 'black';
ctx.shadowOffsetX = 10;
ctx.shadowOffsetY = 10;
ctx.shadowBlur = 0;
ctx.fillStyle = '#ff0000';
ctx.fillRect(10, 10, 60, 60);

// 2. Blue shadow offset
ctx.shadowColor = '#0000ff';
ctx.shadowOffsetX = 8;
ctx.shadowOffsetY = 8;
ctx.fillStyle = '#00cc00';
ctx.fillRect(120, 10, 60, 60);

// 3. No shadow (verify reset)
ctx.shadowColor = 'transparent';
ctx.shadowOffsetX = 0;
ctx.shadowOffsetY = 0;
ctx.fillStyle = '#9900cc';
ctx.fillRect(60, 120, 80, 50);
