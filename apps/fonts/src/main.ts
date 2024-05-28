import aPath from './fonts/Alexandria.ttf';
import tPath from './fonts/Twemoji.ttf';

const fontData = Switch.readFileSync(new URL(aPath, Switch.entrypoint));
const font = new FontFace('Alexandria', fontData!);
fonts.add(font);

const emojiFontData = Switch.readFileSync(new URL(tPath, Switch.entrypoint));
const emojiFont = new FontFace('Twemoji', emojiFontData!);
fonts.add(emojiFont);

const ctx = screen.getContext('2d');
ctx.textAlign = 'center';

const x = screen.width / 2;
let y = 140;
ctx.font = '48px Alexandria';
ctx.fillStyle = 'purple';
ctx.fillText('This is the "Alexandria" font', x, y);

ctx.font = '28px Alexandria';
ctx.fillStyle = 'rgb(230, 200, 200)';
ctx.fillText('Created by "Teaito"', x, (y += 60));

ctx.font = '26px system-ui';
ctx.fillStyle = 'white';
ctx.fillText('"system-icons" font renders Switch native icons', x, (y += 120));
ctx.font = '100px system-icons';
ctx.fillText('\uE130\uE122\uE12C\uE0C4', x, (y += 114));

ctx.font = '26px system-ui';
ctx.fillStyle = 'orange';
ctx.fillText('Emoji fonts work too!', x, (y += 120));
ctx.font = '100px Twemoji';
ctx.fillText('🇧🇷👩🏾‍🦱🥰🏀', x, (y += 100));

ctx.textAlign = 'right';
ctx.font = '24px system-ui';
ctx.strokeStyle = 'rgb(200, 200, 230)';
ctx.strokeText('Press + to exit', screen.width - 30, 30);
