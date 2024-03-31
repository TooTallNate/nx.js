const ctx = screen.getContext('2d');
ctx.font = '24px system-ui';
ctx.fillStyle = 'white';
ctx.textAlign = 'center';
ctx.textBaseline = 'middle';

const gap = 48;
let x = gap;
let y = gap;
for (const { icon, name } of Switch.Application) {
	if (!icon) continue;
	const img = new Image();
	img.onload = () => {
		if (x + img.width >= screen.width) {
			x = gap;
			y += img.height + 40 + gap;
		}
		ctx.drawImage(img, x, y);
		ctx.fillText(name, x + img.width / 2, y + img.height + 20, img.width);
		x += img.width + gap;
	};
	const url = URL.createObjectURL(new Blob([icon]));
	img.src = url;
	//app.launch()
}
