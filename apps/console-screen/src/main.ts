// Verifies two things that hadn't been exercised together:
//
//   1. console.log(...) first  ->  later screen.getContext('2d') takes over.
//      The console auto-presents to the full screen until the app acquires the
//      screen's 2D context, at which point the console auto-present stops and the
//      app owns rendering.
//
//   2. Compositing console.canvas with the app's own drawing: the app draws its
//      own scene each frame AND blits console.canvas (scaled + rotated) via
//      ctx.drawImage(console.canvas, ...). Crucially, the app keeps logging
//      AFTER the handoff (a per-second counter), proving console.canvas stays
//      LIVE (render-on-demand) and isn't frozen at the last auto-presented frame.
//
// Timeline: ~3s of plain console output (full-screen console), then the app
// takes the screen and animates a scene with a LARGE live console panel (left)
// plus a scaled-down + rotated thumbnail (corner). Press + to exit.

// ---- Phase 1: plain console output (console owns the full screen) ----
console.log('console-screen verification');
console.log('');
console.log('Phase 1: this is the full-screen console.');
console.log('In ~3s the app takes the screen via getContext("2d").');
console.log('');

const W = screen.width; // 1280
const H = screen.height; // 720

// Grab console.canvas before the handoff (stays valid afterwards).
const consoleCanvas = console.canvas;

let started = 0;
let appOwns = false;
let ctx: CanvasRenderingContext2D | null = null;
let frame = 0;
let secs = 0;
let lastTick = 0;

function render(t: number) {
	if (!started) started = t;
	const elapsed = t - started;

	// Phase 1: leave the console alone (it auto-presents itself).
	if (!appOwns && elapsed < 3000) {
		requestAnimationFrame(render);
		return;
	}

	// Phase 2: take over the screen exactly once.
	if (!appOwns) {
		ctx = screen.getContext('2d'); // handoff: console auto-present stops
		appOwns = true;
		lastTick = t;
		console.log('-- App owns the screen now. Live counter below: --');
	}
	if (!ctx) {
		requestAnimationFrame(render);
		return;
	}
	frame++;

	// Keep logging AFTER the handoff: one line per second. If console.canvas is
	// live, these lines keep appearing in the on-screen console panel; if it
	// were frozen, the panel would stop at the handoff message.
	if (t - lastTick >= 1000) {
		lastTick += 1000;
		secs++;
		console.log(`  tick ${secs}: console.canvas is LIVE after handoff`);
	}

	// --- The app's own scene: animated gradient + bouncing circle. ---
	const g = ctx.createLinearGradient(0, 0, W, H);
	g.addColorStop(0, '#101822');
	g.addColorStop(1, `rgb(${40 + (frame % 120)}, 30, 110)`);
	ctx.fillStyle = g;
	ctx.fillRect(0, 0, W, H);

	const cx = W * 0.72 + Math.cos(frame / 30) * 150;
	const cy = H / 2 + Math.sin(frame / 20) * 200;
	ctx.fillStyle = '#ffcc33';
	ctx.beginPath();
	ctx.arc(cx, cy, 40, 0, Math.PI * 2);
	ctx.fill();

	ctx.fillStyle = '#ffffff';
	ctx.font = '26px sans-serif';
	ctx.fillText('App-owned screen (gradient + circle drawn by the app)', 30, 40);

	// --- LARGE live console panel (left half), so the updating text is
	// readable. drawImage(console.canvas) scaled down ~0.5x, upright. ---
	const pw = 600;
	const ph = (H * pw) / W; // keep aspect
	const px = 30;
	const py = 70;
	ctx.fillStyle = 'rgba(0,0,0,0.55)';
	ctx.fillRect(px - 6, py - 6, pw + 12, ph + 12);
	ctx.drawImage(consoleCanvas, px, py, pw, ph);
	ctx.fillStyle = '#9fe09f';
	ctx.font = '20px sans-serif';
	ctx.fillText('^ live console.canvas (drawImage, scaled)', px, py + ph + 28);

	// --- Small rotated thumbnail (corner): drawImage with rotation. ---
	const sc = 280 / W;
	const tw = W * sc;
	const th = H * sc;
	ctx.save();
	ctx.translate(W - tw / 2 - 50, H - th / 2 - 50);
	ctx.rotate(-0.12);
	ctx.fillStyle = 'rgba(0,0,0,0.6)';
	ctx.fillRect(-tw / 2 - 5, -th / 2 - 5, tw + 10, th + 10);
	ctx.drawImage(consoleCanvas, -tw / 2, -th / 2, tw, th);
	ctx.restore();

	requestAnimationFrame(render);
}

requestAnimationFrame(render);
