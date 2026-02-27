// Shadow support: shadowColor, shadowBlur, shadowOffsetX, shadowOffsetY
var c = createCanvas(200, 200);
var ctx = c.ctx;

// White background so shadows are visible
ctx.fillStyle = 'white';
ctx.fillRect(0, 0, 200, 200);

// 1. Hard black shadow (no blur) - fillRect
ctx.shadowColor = 'rgba(0, 0, 0, 0.8)';
ctx.shadowOffsetX = 10;
ctx.shadowOffsetY = 10;
ctx.shadowBlur = 0;
ctx.fillStyle = '#ff0000';
ctx.fillRect(15, 15, 65, 65);

// 2. Blurred blue shadow - fillRect
ctx.shadowColor = 'rgba(0, 0, 255, 0.7)';
ctx.shadowOffsetX = 8;
ctx.shadowOffsetY = 8;
ctx.shadowBlur = 12;
ctx.fillStyle = '#00cc00';
ctx.fillRect(120, 15, 65, 65);

// 3. Large red blurred shadow - circle
ctx.shadowColor = 'rgba(255, 0, 0, 0.6)';
ctx.shadowOffsetX = 6;
ctx.shadowOffsetY = 6;
ctx.shadowBlur = 15;
ctx.fillStyle = '#3366ff';
ctx.beginPath();
ctx.arc(55, 150, 35, 0, Math.PI * 2);
ctx.fill();

// 4. No shadow (verify reset)
ctx.shadowColor = 'transparent';
ctx.shadowOffsetX = 0;
ctx.shadowOffsetY = 0;
ctx.shadowBlur = 0;
ctx.fillStyle = '#ff9900';
ctx.fillRect(110, 120, 70, 50);
