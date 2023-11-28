const ctx = screen.getContext('2d');

const fontUrl = new URL('fonts/Alexandria.ttf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Alexandria', fontData);
Switch.fonts.add(font);

const emojiFontUrl = new URL('fonts/Twemoji.ttf', Switch.entrypoint);
const emojiFontData = Switch.readFileSync(emojiFontUrl);
const emojiFont = new FontFace('Twemoji', emojiFontData);
Switch.fonts.add(emojiFont);

ctx.font = '36px Alexandria';
ctx.fillStyle = 'white';
ctx.fillText('This is the "Alexandria" font', 330, 250);

ctx.font = '28px Alexandria';
ctx.fillStyle = 'rgb(230, 200, 200)';
ctx.fillText('Created by "Teaito"', 460, 310);

ctx.font = '26px system-ui';
ctx.fillStyle = 'orange';
ctx.fillText('Emoji fonts work too!', 480, 480);

ctx.font = '100px Twemoji';
ctx.fillText('🤣', 340, 600);
ctx.fillText('🚀', 490, 600);
ctx.fillText('❤', 640, 600);
ctx.fillText('🏀', 790, 600);

ctx.font = '24px system-ui';
ctx.fillStyle = 'rgb(200, 200, 230)';
ctx.fillText('Press + to exit', screen.width - 190, 30);
