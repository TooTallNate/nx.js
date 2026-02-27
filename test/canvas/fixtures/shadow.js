// Shadow support: shadowColor, shadowBlur, shadowOffsetX, shadowOffsetY
var c = createCanvas(300, 300);
var ctx = c.ctx;

// 1. Simple shadow offset (no blur)
ctx.shadowColor = 'rgba(0, 0, 0, 0.5)';
ctx.shadowOffsetX = 5;
ctx.shadowOffsetY = 5;
ctx.fillStyle = '#ff0000';
ctx.fillRect(20, 20, 80, 80);

// 2. Shadow with blur
ctx.shadowColor = 'rgba(0, 0, 255, 0.7)';
ctx.shadowBlur = 10;
ctx.shadowOffsetX = 8;
ctx.shadowOffsetY = 8;
ctx.fillStyle = '#00cc00';
ctx.fillRect(140, 20, 80, 80);

// 3. Shadow on stroked shapes
ctx.shadowColor = 'rgba(255, 0, 0, 0.6)';
ctx.shadowBlur = 5;
ctx.shadowOffsetX = 4;
ctx.shadowOffsetY = 4;
ctx.strokeStyle = '#0000ff';
ctx.lineWidth = 3;
ctx.strokeRect(20, 140, 80, 80);

// 4. Shadow on path (arc)
ctx.shadowColor = 'rgba(0, 128, 0, 0.8)';
ctx.shadowBlur = 15;
ctx.shadowOffsetX = 6;
ctx.shadowOffsetY = 6;
ctx.fillStyle = '#ff8800';
ctx.beginPath();
ctx.arc(220, 180, 40, 0, Math.PI * 2);
ctx.fill();

// 5. No shadow (reset)
ctx.shadowColor = 'rgba(0, 0, 0, 0)';
ctx.shadowBlur = 0;
ctx.shadowOffsetX = 0;
ctx.shadowOffsetY = 0;
ctx.fillStyle = '#9900cc';
ctx.fillRect(100, 240, 60, 40);
