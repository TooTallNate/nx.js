const ctx = Switch.screen.getContext('2d');

const fontUrl = new URL('Alexandria.ttf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Alexandria', fontData);
Switch.fonts.add(font);

ctx.fillRect(0, 0, Switch.screen.width, Switch.screen.height);

ctx.font = '36px Alexandria';
ctx.fillStyle = 'white';
ctx.fillText('This is the "Alexandria" font', 330, 330);

ctx.font = '28px Alexandria';
ctx.fillStyle = 'rgb(230, 200, 200)';
ctx.fillText('Created by "Teaito"', 460, 390);

ctx.font = '24px system-ui';
ctx.fillStyle = 'rgb(200, 200, 230)';
ctx.fillText('Press + to exit', Switch.screen.width - 190, 30);
