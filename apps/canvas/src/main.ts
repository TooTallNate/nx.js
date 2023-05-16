const ctx = Switch.screen.getContext('2d');
//ctx.fillStyle = 'blue';
//ctx.fillRect(10, 10, 100, 100);

let r = 0;
const imageData = ctx.getImageData(0, 0, 50, 50);
//const d = new Uint32Array(imageData.data);

for (let y = 0; y < imageData.width; y++) {
	for (let x = 0; x < imageData.height; x++) {
		const pos = y * imageData.width * 4 + x * 4;
		//d[pos] = 0xff0000ff; // 0xRRGGBBAA
		//console.log({ x, y, pos });
		imageData.data[pos] = 255; // r
		imageData.data[pos + 1] = 0; // g
		imageData.data[pos + 2] = 0; // b
		imageData.data[pos + 3] = 255; // a
	}
}
//console.log(imageData);

ctx.putImageData(imageData, 0, 0);
ctx.putImageData(imageData, 100, 100);

//Switch.addEventListener('frame', () => {
//    r++;
//	//for (let i = 0; i < 1000; i++) {
//	//	r++;
//	//	if (r >= data.byteLength - 4) {
//	//		r = 0;
//	//	}
//	//	const pos = r * 4;
//	//	data[pos + 0] = r % 255; // r
//	//}
//	for (let y = 0; y < r; y++) {
//	    for (let x = 0; x < Switch.screen.width; x++) {
//	        const pos = (y * Switch.screen.width) + (x);
//	        data[pos] = 0x00ffff00; // 0xRRGGBBAA
//	        //data[pos] = r % 255; // r
//	        //data[pos + 1] = 0; // g
//	        //data[pos + 2] = 0; // b
//	        //data[pos + 3] = 0; // a
//	    }
//	}
//	//console.log(r);
//});
