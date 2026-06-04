// Path coordinates must bake in the CTM at the time each segment is added,
// independent of the CTM in effect when the path is later filled/stroked
// (HTML Canvas spec). These cases build a path under one transform, then change
// the transform before drawing — exercising the build-time-vs-draw-time CTM
// distinction that a naive "store user-space path, apply CTM at draw" model
// gets wrong.
var c = createCanvas(200, 200);
var ctx = c.ctx;

// 1. rect built under translate, then transform reset before fill.
//    The rect must appear where it was built (60,10), not at the origin.
ctx.fillStyle = '#cc3333';
ctx.save();
ctx.translate(60, 10);
ctx.beginPath();
ctx.rect(0, 0, 40, 30);
ctx.restore(); // CTM back to identity
ctx.fill(); // must fill at (60,10)..(100,40)

// 2. path built across two different transforms (scale, then translate),
//    then stroked under identity.
ctx.strokeStyle = '#3366cc';
ctx.lineWidth = 4;
ctx.save();
ctx.beginPath();
ctx.translate(20, 70);
ctx.scale(2, 1);
ctx.moveTo(0, 0); // -> device (20,70)
ctx.lineTo(30, 0); // -> device (80,70)
ctx.translate(0, 30);
ctx.lineTo(30, 0); // -> device (80,100)
ctx.restore();
ctx.stroke();

// 3. rotated square path, drawn after the rotation is undone.
ctx.fillStyle = 'rgba(51,153,51,0.8)';
ctx.save();
ctx.translate(140, 90);
ctx.rotate(Math.PI / 4);
ctx.beginPath();
ctx.rect(-20, -20, 40, 40);
ctx.restore();
ctx.fill(); // diamond centered at (140,90)

// 4. clip built under transform, fill after reset — clip region must stay put.
ctx.save();
ctx.translate(40, 140);
ctx.beginPath();
ctx.rect(0, 0, 50, 50);
ctx.restore();
ctx.clip();
ctx.fillStyle = '#ff9900';
ctx.fillRect(0, 0, 200, 200); // only (40,140)..(90,190) should paint
