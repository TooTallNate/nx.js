// roundRect with scaling transform applied — regression test for
// incorrect radius component scaling (upperRight.y, lowerLeft.x,
// lowerRight.x were not being scaled).
var c = createCanvas(200, 200);
var ctx = c.ctx;

// 1) Uniform scale — symmetric radii should stay symmetric
ctx.save();
ctx.scale(2, 2);
ctx.fillStyle = '#3366cc';
ctx.beginPath();
ctx.roundRect(5, 5, 40, 30, 10);
ctx.fill();
ctx.restore();

// 2) Non-uniform scale — asymmetric radii per-corner
//    This is the key case: x and y radius components scale differently,
//    so if the wrong component is scaled the shape will be visibly wrong.
ctx.save();
ctx.scale(1.5, 0.75);
ctx.fillStyle = '#cc3333';
ctx.beginPath();
ctx.roundRect(5, 100, 80, 80, [10, 20, 30, 5]);
ctx.fill();
ctx.restore();

// 3) Scale + stroke — ensures stroke path also gets correct radii
ctx.save();
ctx.scale(0.5, 1.5);
ctx.strokeStyle = '#339933';
ctx.lineWidth = 4;
ctx.beginPath();
ctx.roundRect(220, 10, 140, 50, 20);
ctx.stroke();
ctx.restore();

// 4) Scale with setTransform — absolute transform
ctx.setTransform(2, 0, 0, 0.5, 0, 140);
ctx.fillStyle = '#993399';
ctx.beginPath();
ctx.roundRect(10, 10, 60, 80, [15, 5, 25, 10]);
ctx.fill();
