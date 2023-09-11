import { Slider } from './slider';

const MAX_FREQ = 320;

const canvas = Switch.screen;
const ctx = canvas.getContext('2d');
ctx.fillStyle = 'black';
ctx.fillRect(0, 0, canvas.width, canvas.height);

ctx.fillStyle = 'white';
ctx.font = '48px system-ui';
const m = ctx.measureText('Vibration Demo');
ctx.fillText('Vibration Demo', canvas.width / 2 - m.width / 2, 50);

const lowAmpX = (canvas.width / 5) * 1;
const lowAmpSlider = new Slider(ctx, lowAmpX, 100, 400);
lowAmpSlider.value = 0.2;

const lowFreqX = (canvas.width / 5) * 2;
const lowFreqSlider = new Slider(ctx, lowFreqX, 100, 400);
lowFreqSlider.value = 0.5;

const highAmpX = (canvas.width / 5) * 3;
const highAmpSlider = new Slider(ctx, highAmpX, 100, 400);
highAmpSlider.value = 0.2;

const highFreqX = (canvas.width / 5) * 4;
const highFreqSlider = new Slider(ctx, highFreqX, 100, 400);
highFreqSlider.value = 0.5;

const sliders = [lowAmpSlider, lowFreqSlider, highAmpSlider, highFreqSlider];

ctx.font = '36px system-ui';
ctx.fillStyle = 'white';
ctx.fillText('Low', lowAmpX + 110, 318);
ctx.fillText('Amp', lowAmpX - 30, 100 + lowAmpSlider.length + 60);
ctx.fillText('Freq', lowFreqX - 30, 100 + lowFreqSlider.length + 60);

ctx.fillText('High', highAmpX + 100, 318);
ctx.fillText('Amp', highAmpX - 30, 100 + highAmpSlider.length + 60);
ctx.fillText('Freq', highFreqX - 30, 100 + highFreqSlider.length + 60);

Switch.addEventListener('touchmove', (e) => {
	const t = e.touches[0];
	const x = t.clientX;
	const y = t.clientY;
	for (const s of sliders) {
		const rect = {
			x: s.x,
			y: s.y,
			width: 20,
			height: s.length,
		};
		if (
			x >= rect.x - rect.width / 2 &&
			x <= rect.x + rect.width + rect.width / 2 &&
			y >= rect.y - rect.height / 2 &&
			y <= rect.y + rect.height + rect.width / 2
		) {
			const v = 1 - (y - s.y) / s.length;
			s.value = v;

			const vs =
				s === lowFreqSlider || s === highFreqSlider
					? `${Math.round(s.value * MAX_FREQ)} Hz`
					: `${Math.round(s.value * 100)}%`;
			ctx.font = '30px system-ui';

			ctx.beginPath();
			ctx.rect(rect.x - 20, rect.y + rect.height + 80, 110, 34);
			ctx.fillStyle = 'black';
			ctx.fill();

			ctx.fillStyle = 'white';
			ctx.fillText(vs, rect.x - 20, rect.y + rect.height + 110);

			Switch.vibrate({
				duration: 1000,
				lowAmp: lowAmpSlider.value,
				lowFreq: Math.round(lowFreqSlider.value * MAX_FREQ),
				highAmp: highAmpSlider.value,
				highFreq: Math.round(highFreqSlider.value * MAX_FREQ),
			});
		}
	}
});
