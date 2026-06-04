// Drawing works correctly after resize
// Resize the canvas to different dimensions, then draw various shapes
// to verify the rendering pipeline is fully functional.
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Draw initial content
ctx.fillStyle = '#ff0000';
ctx.fillRect(0, 0, 200, 200);

// Resize to smaller, then back to original
c.canvas.width = 100;
c.canvas.height = 100;
c.canvas.width = 200;
c.canvas.height = 200;

// Draw after resize — should work correctly on a fresh canvas
ctx.fillStyle = '#3366cc';
ctx.fillRect(20, 20, 60, 60);

ctx.fillStyle = '#cc6633';
ctx.beginPath();
ctx.arc(140, 50, 30, 0, Math.PI * 2);
ctx.fill();

ctx.fillStyle = '#33cc66';
ctx.fillRect(20, 120, 160, 60);

ctx.fillStyle = '#ffffff';
ctx.fillRect(40, 140, 120, 20);
