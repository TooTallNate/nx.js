const ctx = Switch.screen.getContext('2d');
//ctx.fillStyle = 'blue';
//ctx.fillRect(10, 10, 100, 100);

let r = 0;
const data = ctx.getImageData();

Switch.addEventListener('frame', () => {
	r++;
	const pos = r * 4;
	data[pos + 0] = r % 255; // r
	//for (let y = 0; y < 20; y++) {
	//    for (let x = 0; x < Switch.screen.width; x++) {
	//        const pos = (y * Switch.screen.width * 4) + (x * 4);
	//        data[pos + 0] = r % 255; // r
	//        //data[pos + 1] = 0; // g
	//        //data[pos + 2] = 0; // b
	//        //data[pos + 3] = 0; // a
	//    }
	//}
	//console.log(r);
});

//for (let i = 0; i < 15; i++) {
//    console.log(data[i]);
//}
