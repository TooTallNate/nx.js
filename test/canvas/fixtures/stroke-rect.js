// strokeRect with various line widths and colors
var c = createCanvas(200, 200);
var ctx = c.ctx;

ctx.strokeStyle = '#ff0000';
ctx.lineWidth = 1;
ctx.strokeRect(10, 10, 60, 60);

ctx.strokeStyle = '#00ff00';
ctx.lineWidth = 3;
ctx.strokeRect(80, 10, 60, 60);

ctx.strokeStyle = '#0000ff';
ctx.lineWidth = 5;
ctx.strokeRect(30, 80, 60, 60);

ctx.strokeStyle = 'rgba(255, 0, 255, 0.7)';
ctx.lineWidth = 8;
ctx.strokeRect(100, 80, 80, 80);
