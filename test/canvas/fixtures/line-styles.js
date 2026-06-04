// lineCap, lineJoin, miterLimit
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Line caps: butt, round, square
var caps = ['butt', 'round', 'square'];
ctx.strokeStyle = '#333333';
ctx.lineWidth = 12;

for (var i = 0; i < caps.length; i++) {
	ctx.lineCap = caps[i];
	ctx.beginPath();
	ctx.moveTo(30, 30 + i * 30);
	ctx.lineTo(170, 30 + i * 30);
	ctx.stroke();
}

// Reference lines to show cap extension
ctx.strokeStyle = '#cccccc';
ctx.lineWidth = 1;
ctx.lineCap = 'butt';
ctx.beginPath();
ctx.moveTo(30, 15);
ctx.lineTo(30, 105);
ctx.moveTo(170, 15);
ctx.lineTo(170, 105);
ctx.stroke();

// Line joins: miter, round, bevel
var joins = ['miter', 'round', 'bevel'];
ctx.lineWidth = 10;
ctx.strokeStyle = '#0066cc';

for (var j = 0; j < joins.length; j++) {
	ctx.lineJoin = joins[j];
	ctx.beginPath();
	ctx.moveTo(20 + j * 65, 120);
	ctx.lineTo(40 + j * 65, 170);
	ctx.lineTo(60 + j * 65, 120);
	ctx.stroke();
}
