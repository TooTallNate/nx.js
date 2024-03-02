screen.enable1080p();

const ctx = screen.getContext('2d');

function clock() {
	const now = new Date();
	const scaleFactor = screen.height / 150;

	// Reset
	ctx.clearRect(0, 0, screen.width, screen.height);
	ctx.fillStyle = 'white';

	// Render time as text
	ctx.font = `${8 * scaleFactor}px system-ui`;
	ctx.textAlign = 'center';
	ctx.textBaseline = 'bottom';
	ctx.fillText(now.toLocaleString(), screen.width / 2, screen.height);

	ctx.save();

	ctx.translate(
		screen.width / 2 - (150 * scaleFactor) / 2,
		screen.height / 2 - (150 * scaleFactor) / 2
	);
	ctx.scale(scaleFactor, scaleFactor);
	ctx.translate(75, 75);
	ctx.scale(0.4, 0.4);
	ctx.rotate(-Math.PI / 2);
	ctx.strokeStyle = 'white';
	ctx.lineWidth = 8;
	ctx.lineCap = 'round';

	// Hour marks
	ctx.save();
	for (let i = 0; i < 12; i++) {
		ctx.beginPath();
		ctx.rotate(Math.PI / 6);
		ctx.moveTo(100, 0);
		ctx.lineTo(120, 0);
		ctx.stroke();
	}
	ctx.restore();

	// Minute marks
	ctx.save();
	ctx.lineWidth = 5;
	for (let i = 0; i < 60; i++) {
		if (i % 5 !== 0) {
			ctx.beginPath();
			ctx.moveTo(117, 0);
			ctx.lineTo(120, 0);
			ctx.stroke();
		}
		ctx.rotate(Math.PI / 30);
	}
	ctx.restore();

	const sec = now.getSeconds() + now.getMilliseconds() / 1000;
	const min = now.getMinutes();
	const hr = now.getHours() % 12;

	// Write Hours
	ctx.save();
	ctx.rotate(
		(Math.PI / 6) * hr + (Math.PI / 360) * min + (Math.PI / 21600) * sec
	);
	ctx.lineWidth = 14;
	ctx.beginPath();
	ctx.moveTo(-20, 0);
	ctx.lineTo(80, 0);
	ctx.stroke();
	ctx.restore();

	// Write Minutes
	ctx.save();
	ctx.rotate((Math.PI / 30) * min + (Math.PI / 1800) * sec);
	ctx.lineWidth = 10;
	ctx.beginPath();
	ctx.moveTo(-28, 0);
	ctx.lineTo(112, 0);
	ctx.stroke();
	ctx.restore();

	// Write seconds
	ctx.save();
	ctx.rotate((sec * Math.PI) / 30);
	ctx.strokeStyle = '#D40000';
	ctx.fillStyle = '#D40000';
	ctx.lineWidth = 6;
	ctx.beginPath();
	ctx.moveTo(-30, 0);
	ctx.lineTo(83, 0);
	ctx.stroke();
	ctx.beginPath();
	ctx.arc(0, 0, 10, 0, Math.PI * 2, true);
	ctx.fill();
	ctx.beginPath();
	ctx.arc(95, 0, 10, 0, Math.PI * 2, true);
	ctx.stroke();
	ctx.restore();

	ctx.beginPath();
	ctx.lineWidth = 14;
	ctx.strokeStyle = '#325FA2';
	ctx.arc(0, 0, 142, 0, Math.PI * 2, true);
	ctx.stroke();

	ctx.restore();

	window.requestAnimationFrame(clock);
}

window.requestAnimationFrame(clock);
