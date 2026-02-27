// Test toDataURL: draw a pattern, verify the data URL output format
var c = createCanvas(200, 200);
var ctx = c.ctx;

// Draw a recognizable quad pattern
ctx.fillStyle = '#ff0000';
ctx.fillRect(0, 0, 100, 100);
ctx.fillStyle = '#00ff00';
ctx.fillRect(100, 0, 100, 100);
ctx.fillStyle = '#0000ff';
ctx.fillRect(0, 100, 100, 100);
ctx.fillStyle = '#ffff00';
ctx.fillRect(100, 100, 100, 100);

// Test PNG (default)
var pngUrl = c.canvas.toDataURL();
if (typeof pngUrl !== 'string') throw new Error('toDataURL must return a string');
if (!pngUrl.startsWith('data:image/png;base64,')) throw new Error('Default should be PNG');
if (pngUrl.length < 50) throw new Error('Data URL too short');

// Test explicit PNG
var pngUrl2 = c.canvas.toDataURL('image/png');
if (!pngUrl2.startsWith('data:image/png;base64,')) throw new Error('Explicit PNG failed');

// Test JPEG
var jpegUrl = c.canvas.toDataURL('image/jpeg', 0.8);
if (!jpegUrl.startsWith('data:image/jpeg;base64,')) throw new Error('JPEG encoding failed');

// Test unsupported type falls back to PNG
var bmpUrl = c.canvas.toDataURL('image/bmp');
if (!bmpUrl.startsWith('data:image/png;base64,')) throw new Error('Unsupported type should fall back to PNG');
