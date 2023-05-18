const ctx = Switch.screen.getContext('2d');

ctx.fillStyle = 'rgb(0, 0, 76.5)';
ctx.fillRect(0, 0, Switch.screen.width, Switch.screen.height);

ctx.fillStyle = 'red';
ctx.fillRect(0, 0, 100, 100);

ctx.fillStyle = 'green';
ctx.fillRect(200, 200, 100, 100);

ctx.fillStyle = 'white';
ctx.fillText('Hello, from JS!', 100, 100);
