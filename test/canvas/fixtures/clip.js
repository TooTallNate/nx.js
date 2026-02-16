// Clipping regions
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Circular clip with filled background
ctx.save();
ctx.beginPath();
ctx.arc(70, 70, 50, 0, Math.PI * 2);
ctx.clip();

// Fill the entire canvas — only the clipped area shows
ctx.fillStyle = '#3366cc';
ctx.fillRect(0, 0, 200, 200);

// Draw stripes inside clip
ctx.fillStyle = '#ffffff';
for (var i = 0; i < 10; i++) {
	ctx.fillRect(i * 20, 0, 10, 200);
}
ctx.restore();

// Rectangular clip
ctx.save();
ctx.beginPath();
ctx.rect(110, 30, 70, 70);
ctx.clip();

ctx.fillStyle = '#cc3333';
ctx.fillRect(0, 0, 200, 200);

ctx.fillStyle = '#ffcc00';
ctx.beginPath();
ctx.arc(145, 65, 40, 0, Math.PI * 2);
ctx.fill();
ctx.restore();

// After restore — no clip, draw freely
ctx.fillStyle = '#009933';
ctx.fillRect(60, 140, 80, 40);
