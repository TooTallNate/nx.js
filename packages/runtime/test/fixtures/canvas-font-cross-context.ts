import { test } from '../src/tap';

// Regression tests for cross-context font size leakage (PR #406).
//
// In nx.js, all 2D contexts that resolve the same font share one
// FreeType FT_Face + HarfBuzz hb_font. FT_Face's char_size is
// device-global, so a save()/restore() on one context (whose restore
// re-pins the popped state's font_size onto the shared face) used to
// corrupt the effective font size of every other context sharing the
// font — until that other context happened to re-assign a *different*
// font string (same-string assignment short-circuits in the setter).
//
// Each test measures/draws on context B at 14px, runs a save/font/
// restore dance on context A (whose outer state is the default
// "10px sans-serif"), then measures/draws on B again. The results
// must be identical.

const TEXT = 'Hello nx.js';

/** Count pixels with non-zero alpha (ink coverage). */
function inkCoverage(
	ctx: OffscreenCanvasRenderingContext2D,
	w: number,
	h: number,
): number {
	const d = ctx.getImageData(0, 0, w, h).data;
	let n = 0;
	for (let i = 3; i < d.length; i += 4) {
		if (d[i] !== 0) n++;
	}
	return n;
}

test('measureText unaffected by save/restore on another context', (t) => {
	const a = new OffscreenCanvas(200, 50).getContext('2d')!;
	const b = new OffscreenCanvas(200, 50).getContext('2d')!;

	b.font = '14px sans-serif';
	const before = b.measureText(TEXT).width;
	t.ok(before > 0, 'baseline measureText width is positive');

	// Context A: outer state has the default "10px sans-serif";
	// inner scope sets 20px, draws, and restores back to 10px.
	a.save();
	a.font = '20px sans-serif';
	a.fillText('A', 10, 30);
	a.restore();

	const after = b.measureText(TEXT).width;
	t.equal(
		after,
		before,
		'measureText width on context B is unchanged after context A save/restore',
	);
});

test('fillText unaffected by save/restore on another context', (t) => {
	const a = new OffscreenCanvas(200, 50).getContext('2d')!;
	const b = new OffscreenCanvas(200, 50).getContext('2d')!;

	b.font = '14px sans-serif';
	b.fillText(TEXT, 10, 30);
	const before = inkCoverage(b, 200, 50);
	t.ok(before > 0, 'baseline fillText produced ink');

	a.save();
	a.font = '20px sans-serif';
	a.fillText('A', 10, 30);
	a.restore();

	b.clearRect(0, 0, 200, 50);
	b.fillText(TEXT, 10, 30);
	const after = inkCoverage(b, 200, 50);
	t.equal(
		after,
		before,
		'fillText ink coverage on context B is unchanged after context A save/restore',
	);
});

test('strokeText unaffected by save/restore on another context', (t) => {
	const a = new OffscreenCanvas(200, 50).getContext('2d')!;
	const b = new OffscreenCanvas(200, 50).getContext('2d')!;

	b.font = '14px sans-serif';
	b.strokeText(TEXT, 10, 30);
	const before = inkCoverage(b, 200, 50);
	t.ok(before > 0, 'baseline strokeText produced ink');

	a.save();
	a.font = '20px sans-serif';
	a.fillText('A', 10, 30);
	a.restore();

	b.clearRect(0, 0, 200, 50);
	b.strokeText(TEXT, 10, 30);
	const after = inkCoverage(b, 200, 50);
	t.equal(
		after,
		before,
		'strokeText ink coverage on context B is unchanged after context A save/restore',
	);
});
