import { Hid, Swkbd } from 'nxjs-constants';
const { Button } = Hid;

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

Switch.addEventListener('buttondown', (e) => {
	const isOpen = vk.boundingRect.height > 0;
	if (isOpen) {
		if (e.detail & Button.Plus) {
			e.preventDefault();
		} else if (e.detail & Button.ZL) {
			vk.hide();
		}
	} else {
		if (e.detail & Button.ZR) {
			vk.type = Swkbd.Type.Normal;
			vk.okButtonText = 'Done';
			vk.show();
		} else if (e.detail & Button.ZL) {
			vk.type = Swkbd.Type.NumPad;
			vk.okButtonText = 'Submit';
			vk.leftButtonText = ':';
			vk.rightButtonText = '.';
			vk.show();
		}
	}
});

vk.addEventListener('change', render);
vk.addEventListener('cursormove', render);
vk.addEventListener('geometrychange', render);
render();
