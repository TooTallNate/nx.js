import gif from './shaq-cat.gif';
import decodeGIF from 'decode-gif';

const ctx = screen.getContext('2d');
const gifData = Switch.readFileSync(new URL(gif, import.meta.url));
const decoded = decodeGIF(new Uint8Array(gifData!));
const frames = decoded.frames.map(
	(frame) => new ImageData(frame.data, decoded.width, decoded.height),
);
const x = screen.width / 2 - decoded.width / 2;
const y = screen.height / 2 - decoded.height / 2;

while (true) {
	let prevTimeCode = 0;
	for (let i = 0; i < frames.length; i++) {
		const { timeCode } = decoded.frames[i];
		ctx.putImageData(frames[i], x, y);
		const timeCodeDelta = timeCode - prevTimeCode;
		prevTimeCode = timeCode;
		await new Promise((r) => setTimeout(r, timeCodeDelta));
	}
}
