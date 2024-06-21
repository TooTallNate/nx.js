enum Button {
	B,
	A,
	Y,
	X,
	L,
	R,
	ZL,
	ZR,
	Minus,
	Plus,
	StickL,
	StickR,
	Up,
	Down,
	Left,
	Right,
}

function update() {
	requestAnimationFrame(update);
	console.print('\x1B[2J\x1B[1G');
	const pads = navigator.getGamepads();
	for (let i = 0; i < 8; i++) {
		const pad = pads[i];
		if (pad) {
			console.log(
				i,
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
}

update();
