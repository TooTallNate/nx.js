// globalCompositeOperation modes
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Draw source (blue circle)
function drawSource() {
	ctx.fillStyle = 'rgba(0, 0, 255, 0.8)';
	ctx.beginPath();
	ctx.arc(35, 15, 20, 0, Math.PI * 2);
	ctx.fill();
}

// Draw destination (red rectangle)
function drawDest() {
	ctx.fillStyle = 'rgba(255, 0, 0, 0.8)';
	ctx.fillRect(0, 0, 30, 30);
}

var ops = ['source-over', 'source-atop', 'destination-over', 'destination-out',
           'lighter', 'xor'];
var col = 0;
var row = 0;

for (var i = 0; i < ops.length; i++) {
	ctx.save();
	ctx.translate(10 + col * 65, 10 + row * 65);

	drawDest();
	ctx.globalCompositeOperation = ops[i];
	drawSource();

	ctx.globalCompositeOperation = 'source-over';
	ctx.restore();

	col++;
	if (col >= 3) {
		col = 0;
		row++;
	}
}
