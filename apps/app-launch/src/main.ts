import { NACP } from '@tootallnate/nacp';

const ctx = screen.getContext('2d');
ctx.font = '24px system-ui';
ctx.fillStyle = 'white';
ctx.textAlign = 'center';
ctx.textBaseline = 'middle';

const gap = 48;
let x = gap;
let y = gap;
for (const app of Switch.applications) {
	const nacp = new NACP(app.nacp);
	const icon = new Image();
	icon.onload = () => {
		if (x + icon.width >= 1280) {
			x = gap;
			y += icon.height + 40 + gap;
		}
		ctx.drawImage(icon, x, y);
		ctx.fillText(
			nacp.title,
			x + icon.width / 2,
			y + icon.height + 20,
			icon.width
		);
		x += icon.width + gap;
	};
	const url = URL.createObjectURL(new Blob([app.icon]));
	icon.src = url;
	//app.launch()
}
