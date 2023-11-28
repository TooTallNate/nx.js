console.log('Hello World!');
console.log(' - <3 nx.js');
console.log('');
console.log('Press the + button to exit...');

const img = new Image();
img.addEventListener('load', () => {
	console.log('image loaded', { w: img.width, h: img.height });
});
img.src = 'https://nxjs.n8.io/assets/logo.png';
