import { Button, HidNpadStyleTag, HidDeviceTypeBits } from '@nx.js/constants';

function update() {
	requestAnimationFrame(update);
	console.print('\x1B[2J\x1B[1G');
	const pads = navigator.getGamepads();
	for (const pad of pads) {
		if (!pad) continue;
		console.log(
			pad.index,
			HidNpadStyleTag[pad.styleSet] ?? pad.styleSet,
			HidDeviceTypeBits[pad.deviceType] ?? pad.deviceType,
			pad.axes.map((v) => v.toFixed(2)),
		);
		const pressed: string[] = [];
		for (let j = 0; j < pad.buttons.length; j++) {
			const b = pad.buttons[j];
			if (b.pressed) {
				pressed.push(Button[j]);
			}
		}
		console.log(`Presed: ${pressed.join(', ')}`);
		console.log();
	}
}

update();
