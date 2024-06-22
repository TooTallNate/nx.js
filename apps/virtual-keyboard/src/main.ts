import { Button, SwkbdType } from '@nx.js/constants';

const ctx = screen.getContext('2d');
const vk = navigator.virtualKeyboard;

function render() {
	const isOpen = vk.boundingRect.height > 0;

	ctx.fillStyle = 'white';
	ctx.fillRect(0, 0, screen.width, screen.height);

	ctx.font = '24px system-ui';
	ctx.fillStyle = '#444';
	ctx.fillText('Press "ZL" to show num keyboard', 10, 32);
	ctx.fillText('Press "ZR" to show text keyboard', 10, 68);

	ctx.font = '32px system-ui';
	ctx.fillStyle = isOpen ? 'black' : '#550';
	ctx.fillText(`${vk.value}`, 10, 120);

	if (isOpen) {
		const { width } = ctx.measureText(vk.value.slice(0, vk.cursorIndex));
		ctx.fillStyle = 'green';
		ctx.fillRect(13 + width, 90, 3, 32);
	}
}

// Prevent "+" from exiting the app while the virtual keyboard is open
addEventListener('beforeunload', (e) => {
	const isOpen = vk.boundingRect.height > 0;
	if (isOpen) {
		e.preventDefault();
	}
});

function handleInput() {
	requestAnimationFrame(handleInput);
	const gp = navigator.getGamepads()[0];
	if (!gp) return;
	const isOpen = vk.boundingRect.height > 0;
	if (isOpen) {
		if (gp.buttons[Button.ZL].pressed) {
			vk.hide();
		}
	} else {
		if (gp.buttons[Button.ZR].pressed) {
			vk.type = SwkbdType.Normal;
			vk.okButtonText = 'Done';
			vk.show();
		} else if (gp.buttons[Button.ZL].pressed) {
			vk.type = SwkbdType.NumPad;
			vk.okButtonText = 'Submit';
			vk.leftButtonText = ':';
			vk.rightButtonText = '.';
			vk.show();
		}
	}
}

vk.addEventListener('change', render);
vk.addEventListener('cursormove', render);
vk.addEventListener('geometrychange', render);
handleInput();
render();
