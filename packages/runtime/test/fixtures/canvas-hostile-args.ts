import { test } from '../src/tap';

// Regression: the native canvas bindings used `.ToLocalChecked()` on property
// reads / number coercions of user-supplied arrays/objects (setLineDash,
// roundRect radii, putImageData). If a getter or valueOf threw, V8 returned an
// empty MaybeLocal and ToLocalChecked() ABORTED the whole process (a hard crash
// that, on a Switch, forces a console restart). They now use `.ToLocal()` and
// propagate the exception as a normal JS throw — matching Chrome.
//
// These use a real Array containing an element whose `valueOf` throws, which
// both V8 (nx.js) and Chrome coerce the same way (so the conformance harness
// can compare them assertion-for-assertion).

function ctx(w = 64, h = 64): OffscreenCanvasRenderingContext2D {
	return new OffscreenCanvas(w, h).getContext('2d')!;
}

// An object that throws when coerced to a number.
function throwingNumber(message: string): number {
	return {
		valueOf() {
			throw new Error(message);
		},
	} as unknown as number;
}

test('setLineDash with a throwing element coercion propagates the error', (t) => {
	const c = ctx();
	t.throws(
		() => c.setLineDash([throwingNumber('dash boom')]),
		/dash boom/,
		'the valueOf exception surfaces as a JS throw (no crash)',
	);
});

// (roundRect and putImageData with hostile objects are intentionally not
// asserted for cross-engine equality: nx.js validates the radius type and
// duck-types ImageData, so it rejects/reads at a different point than Chrome —
// a legitimate behavioral difference. The point of this audit is that none of
// these paths hard-crash; the native `.ToLocal()` change is exercised by the
// setLineDash case above, which matches Chrome exactly.)

// Sanity: well-formed inputs still work after the .ToLocal() conversion.
test('setLineDash with a normal array still works', (t) => {
	const c = ctx();
	c.setLineDash([4, 2]);
	t.deepEqual(c.getLineDash(), [4, 2], 'dash pattern round-trips');
});
