const ctx = screen.getContext('2d');

const fontUrl = new URL('fonts/Alexandria.ttf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Alexandria', fontData);
Switch.fonts.add(font);

//const emojiFontUrl = new URL('fonts/NotoColorEmoji-Regular.ttf', Switch.entrypoint);
const emojiFontUrl = new URL('fonts/Twemoji.ttf', Switch.entrypoint);
const emojiFontData = Switch.readFileSync(emojiFontUrl);
const emojiFont = new FontFace('Twemoji', emojiFontData);
Switch.fonts.add(emojiFont);

function fillTextCentered(text: string, y: number) {
	const { width } = ctx.measureText(text);
	ctx.fillText(text, screen.width / 2 - width / 2, y);
}

ctx.font = '48px Alexandria';
ctx.fillStyle = 'purple';
fillTextCentered('This is the "Alexandria" font', 250);

ctx.font = '28px Alexandria';
ctx.fillStyle = 'rgb(230, 200, 200)';
fillTextCentered('Created by "Teaito"', 310);

ctx.font = '26px system-ui';
ctx.fillStyle = 'orange';
fillTextCentered('Emoji fonts work too!', 480);

ctx.font = '100px Twemoji';
fillTextCentered('🇧🇷👩🏾‍🦱🥰🏀', 600);

ctx.font = '24px system-ui';
ctx.strokeStyle = 'rgb(200, 200, 230)';
ctx.strokeText('Press + to exit', screen.width - 190, 30);
