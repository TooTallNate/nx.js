const ctx = screen.getContext('2d');

const fontUrl = new URL('fonts/Alexandria.ttf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Alexandria', fontData);
fonts.add(font);

//const emojiFontUrl = new URL('fonts/NotoColorEmoji-Regular.ttf', Switch.entrypoint);
const emojiFontUrl = new URL('fonts/Twemoji.ttf', Switch.entrypoint);
const emojiFontData = Switch.readFileSync(emojiFontUrl);
const emojiFont = new FontFace('Twemoji', emojiFontData);
fonts.add(emojiFont);

ctx.textAlign = 'center';

ctx.font = '48px Alexandria';
ctx.fillStyle = 'purple';
ctx.fillText('This is the "Alexandria" font', screen.width / 2, 250);

ctx.font = '28px Alexandria';
ctx.fillStyle = 'rgb(230, 200, 200)';
ctx.fillText('Created by "Teaito"', screen.width / 2, 310);

ctx.font = '26px system-ui';
ctx.fillStyle = 'orange';
ctx.fillText('Emoji fonts work too!', screen.width / 2, 480);

ctx.font = '100px Twemoji';
ctx.fillText('🇧🇷👩🏾‍🦱🥰🏀', screen.width / 2, 600);

ctx.textAlign = 'right';

ctx.font = '24px system-ui';
ctx.strokeStyle = 'rgb(200, 200, 230)';
ctx.strokeText('Press + to exit', screen.width - 30, 30);
