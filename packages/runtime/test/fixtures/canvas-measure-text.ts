import { test } from '../src/tap';

// Tests for measureText() vertical TextMetrics fields.
//
// Before this fix, nx.js returned 0 for every field except `width`.
// The spec requires actual bounding-box metrics, font metrics, and
// baseline offsets.  These tests verify that the returned values are
// structurally correct (non-zero, correct sign conventions) and that
// `textBaseline` shifts the metrics consistently.
//
// Note: emHeightAscent / emHeightDescent are omitted because Chrome
// does not implement them yet, so they would fail the conformance
// comparison (nxjs-test passes but Chrome returns undefined).

function ctx(w = 300, h = 100): OffscreenCanvasRenderingContext2D {
	return new OffscreenCanvas(w, h).getContext('2d')!;
}

test('measureText returns all TextMetrics fields', (t) => {
	const c = ctx();
	c.font = '24px sans-serif';
	const m = c.measureText('Hg');
	// The spec defines these 12 fields on TextMetrics
	t.equal(typeof m.width, 'number', 'width is a number');
	t.equal(typeof m.actualBoundingBoxLeft, 'number', 'actualBoundingBoxLeft is a number');
	t.equal(typeof m.actualBoundingBoxRight, 'number', 'actualBoundingBoxRight is a number');
	t.equal(typeof m.actualBoundingBoxAscent, 'number', 'actualBoundingBoxAscent is a number');
	t.equal(typeof m.actualBoundingBoxDescent, 'number', 'actualBoundingBoxDescent is a number');
	t.equal(typeof m.fontBoundingBoxAscent, 'number', 'fontBoundingBoxAscent is a number');
	t.equal(typeof m.fontBoundingBoxDescent, 'number', 'fontBoundingBoxDescent is a number');
	t.equal(typeof m.hangingBaseline, 'number', 'hangingBaseline is a number');
	t.equal(typeof m.alphabeticBaseline, 'number', 'alphabeticBaseline is a number');
	t.equal(typeof m.ideographicBaseline, 'number', 'ideographicBaseline is a number');
});

test('measureText vertical metrics are non-zero for visible text', (t) => {
	const c = ctx();
	c.font = '24px sans-serif';
	const m = c.measureText('Hg');
	t.ok(m.width > 0, 'width is positive');
	t.ok(m.actualBoundingBoxAscent > 0, 'actualBoundingBoxAscent is positive');
	t.ok(m.actualBoundingBoxDescent > 0, 'actualBoundingBoxDescent is positive (g has a descender)');
	t.ok(m.fontBoundingBoxAscent > 0, 'fontBoundingBoxAscent is positive');
	t.ok(m.fontBoundingBoxDescent > 0, 'fontBoundingBoxDescent is positive');
});

test('measureText empty string returns zero ink but valid font metrics', (t) => {
	const c = ctx();
	c.font = '24px sans-serif';
	const m = c.measureText('');
	t.equal(m.width, 0, 'empty string has zero width');
	t.equal(m.actualBoundingBoxLeft, 0, 'empty string has zero actualBoundingBoxLeft');
	t.equal(m.actualBoundingBoxRight, 0, 'empty string has zero actualBoundingBoxRight');
	t.equal(m.actualBoundingBoxAscent, 0, 'empty string has zero actualBoundingBoxAscent');
	t.equal(m.actualBoundingBoxDescent, 0, 'empty string has zero actualBoundingBoxDescent');
	// Font metrics are still defined even for an empty string
	t.ok(m.fontBoundingBoxAscent > 0, 'empty string still has positive fontBoundingBoxAscent');
	t.ok(m.fontBoundingBoxDescent > 0, 'empty string still has positive fontBoundingBoxDescent');
});

test('measureText font size affects metrics proportionally', (t) => {
	const c = ctx();
	c.font = '12px sans-serif';
	const small = c.measureText('Hg');
	c.font = '24px sans-serif';
	const large = c.measureText('Hg');
	t.ok(large.fontBoundingBoxAscent > small.fontBoundingBoxAscent, 'larger font has larger fontBoundingBoxAscent');
	t.ok(large.fontBoundingBoxDescent > small.fontBoundingBoxDescent, 'larger font has larger fontBoundingBoxDescent');
	t.ok(large.actualBoundingBoxAscent > small.actualBoundingBoxAscent, 'larger font has larger actualBoundingBoxAscent');
});

test('measureText textBaseline shifts font metrics', (t) => {
	const c = ctx();
	c.font = '24px sans-serif';

	c.textBaseline = 'alphabetic';
	const alpha = c.measureText('Hg');

	c.textBaseline = 'top';
	const top = c.measureText('Hg');

	// When textBaseline is 'top', the alignment point moves up to the top of
	// the em box. fontBoundingBoxAscent should decrease (less distance from
	// alignment point to top of font), and fontBoundingBoxDescent should
	// increase (more distance from alignment point to bottom).
	t.ok(top.fontBoundingBoxAscent < alpha.fontBoundingBoxAscent,
		'top baseline has smaller fontBoundingBoxAscent than alphabetic');
	t.ok(top.fontBoundingBoxDescent > alpha.fontBoundingBoxDescent,
		'top baseline has larger fontBoundingBoxDescent than alphabetic');
});

test('measureText alphabeticBaseline is zero at default baseline', (t) => {
	const c = ctx();
	c.font = '24px sans-serif';
	c.textBaseline = 'alphabetic';
	const m = c.measureText('Hg');
	t.equal(m.alphabeticBaseline, 0, 'alphabeticBaseline is zero when textBaseline is alphabetic');
});
