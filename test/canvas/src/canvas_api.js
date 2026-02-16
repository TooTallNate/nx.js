/**
 * Canvas 2D API Bridge for nx.js test harness
 *
 * This script runs inside QuickJS and bridges the standard Canvas 2D API
 * to the native nx.js C bindings exposed via the '$' global (init_obj).
 *
 * The C layer expects:
 *   - fillStyle/strokeStyle: (context, r, g, b, a) where r/g/b are 0-255, a is 0-1
 *   - Most drawing methods: (context, ...args)
 */

// ---- CSS Color Parser ----

var NAMED_COLORS = {
	'black': [0, 0, 0, 1],
	'white': [255, 255, 255, 1],
	'red': [255, 0, 0, 1],
	'green': [0, 128, 0, 1],
	'blue': [0, 0, 255, 1],
	'yellow': [255, 255, 0, 1],
	'cyan': [0, 255, 255, 1],
	'magenta': [255, 0, 255, 1],
	'orange': [255, 165, 0, 1],
	'purple': [128, 0, 128, 1],
	'pink': [255, 192, 203, 1],
	'gray': [128, 128, 128, 1],
	'grey': [128, 128, 128, 1],
	'lime': [0, 255, 0, 1],
	'navy': [0, 0, 128, 1],
	'teal': [0, 128, 128, 1],
	'maroon': [128, 0, 0, 1],
	'olive': [128, 128, 0, 1],
	'aqua': [0, 255, 255, 1],
	'fuchsia': [255, 0, 255, 1],
	'silver': [192, 192, 192, 1],
	'transparent': [0, 0, 0, 0],
	'coral': [255, 127, 80, 1],
	'crimson': [220, 20, 60, 1],
	'gold': [255, 215, 0, 1],
	'indigo': [75, 0, 130, 1],
	'khaki': [240, 230, 140, 1],
	'lavender': [230, 230, 250, 1],
	'salmon': [250, 128, 114, 1],
	'tomato': [255, 99, 71, 1],
	'turquoise': [64, 224, 208, 1],
	'violet': [238, 130, 238, 1],
	'wheat': [245, 222, 179, 1],
	'darkblue': [0, 0, 139, 1],
	'darkgreen': [0, 100, 0, 1],
	'darkred': [139, 0, 0, 1],
	'darkorange': [255, 140, 0, 1],
	'steelblue': [70, 130, 180, 1],
	'dodgerblue': [30, 144, 255, 1],
	'lightblue': [173, 216, 230, 1],
	'lightgreen': [144, 238, 144, 1],
	'lightgray': [211, 211, 211, 1],
	'lightgrey': [211, 211, 211, 1],
	'darkgray': [169, 169, 169, 1],
	'darkgrey': [169, 169, 169, 1],
	'brown': [165, 42, 42, 1],
	'chocolate': [210, 105, 30, 1],
	'firebrick': [178, 34, 34, 1],
	'forestgreen': [34, 139, 34, 1],
	'midnightblue': [25, 25, 112, 1],
	'slategray': [112, 128, 144, 1],
	'slategrey': [112, 128, 144, 1],
};

function parseColor(str) {
	if (typeof str !== 'string') return null;
	str = str.trim().toLowerCase();

	// Named colors
	if (NAMED_COLORS[str]) {
		return NAMED_COLORS[str].slice();
	}

	// #rgb
	var m = str.match(/^#([0-9a-f])([0-9a-f])([0-9a-f])$/);
	if (m) {
		return [
			parseInt(m[1] + m[1], 16),
			parseInt(m[2] + m[2], 16),
			parseInt(m[3] + m[3], 16),
			1
		];
	}

	// #rgba
	m = str.match(/^#([0-9a-f])([0-9a-f])([0-9a-f])([0-9a-f])$/);
	if (m) {
		return [
			parseInt(m[1] + m[1], 16),
			parseInt(m[2] + m[2], 16),
			parseInt(m[3] + m[3], 16),
			parseInt(m[4] + m[4], 16) / 255
		];
	}

	// #rrggbb
	m = str.match(/^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/);
	if (m) {
		return [parseInt(m[1], 16), parseInt(m[2], 16), parseInt(m[3], 16), 1];
	}

	// #rrggbbaa
	m = str.match(/^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/);
	if (m) {
		return [
			parseInt(m[1], 16),
			parseInt(m[2], 16),
			parseInt(m[3], 16),
			parseInt(m[4], 16) / 255
		];
	}

	// rgb(r, g, b) or rgb(r g b)
	m = str.match(/^rgb\(\s*(\d+)\s*[,\s]\s*(\d+)\s*[,\s]\s*(\d+)\s*\)$/);
	if (m) {
		return [parseInt(m[1]), parseInt(m[2]), parseInt(m[3]), 1];
	}

	// rgba(r, g, b, a) or rgba(r g b / a)
	m = str.match(
		/^rgba\(\s*(\d+)\s*[,\s]\s*(\d+)\s*[,\s]\s*(\d+)\s*[,\/]\s*([0-9.]+)\s*\)$/
	);
	if (m) {
		return [
			parseInt(m[1]),
			parseInt(m[2]),
			parseInt(m[3]),
			parseFloat(m[4])
		];
	}

	return null;
}

function rgbaToString(arr) {
	if (!arr) return '#000000';
	var r = arr[0], g = arr[1], b = arr[2], a = arr[3];
	if (a === 1) {
		return 'rgb(' + r + ', ' + g + ', ' + b + ')';
	}
	return 'rgba(' + r + ', ' + g + ', ' + b + ', ' + a + ')';
}

// ---- Canvas Init Class ----

// Initialize the Canvas class so the C side recognizes our objects
var CanvasClass = function() {};
$.canvasInitClass(CanvasClass);

// Initialize the CanvasRenderingContext2D class
var Context2DClass = function() {};
$.canvasContext2dInitClass(Context2DClass);

// ---- CanvasRenderingContext2D Wrapper ----

function createContext2D(canvasObj) {
	// Create the native context via C — this returns an opaque QuickJS class object.
	// canvasContext2dInitClass() installed methods (fillRect, beginPath, etc.) on
	// Context2DClass.prototype, but JS_NewObjectClass() in the C code doesn't
	// automatically link to that prototype. We need to do it explicitly, just like
	// the real nx.js runtime does with Object.setPrototypeOf().
	var nativeCtx = $.canvasContext2dNew(canvasObj);
	Object.setPrototypeOf(nativeCtx, Context2DClass.prototype);

	// The native context now has all the C-backed methods (fillRect, beginPath,
	// save, restore, etc.) via the prototype chain. We wrap it with a thin layer
	// that adds fillStyle/strokeStyle color parsing (the C side expects r,g,b,a
	// components via separate $.canvasContext2dSet*Style calls).

	var _fillStyle = '#000000';
	var _strokeStyle = '#000000';

	Object.defineProperties(nativeCtx, {
		fillStyle: {
			get: function() { return _fillStyle; },
			set: function(v) {
				var parsed = parseColor(v);
				if (!parsed) return;
				_fillStyle = v;
				$.canvasContext2dSetFillStyle(nativeCtx, parsed[0], parsed[1], parsed[2], parsed[3]);
			},
			configurable: true,
		},
		strokeStyle: {
			get: function() { return _strokeStyle; },
			set: function(v) {
				var parsed = parseColor(v);
				if (!parsed) return;
				_strokeStyle = v;
				$.canvasContext2dSetStrokeStyle(nativeCtx, parsed[0], parsed[1], parsed[2], parsed[3]);
			},
			configurable: true,
		},
	});

	return nativeCtx;
}

// ---- createCanvas ----

function createCanvas(width, height) {
	if (width === undefined) width = globalThis.__canvas_width__ || 200;
	if (height === undefined) height = globalThis.__canvas_height__ || 200;

	// Create the native canvas object
	var canvasObj = $.canvasNew(width, height);

	// Store the canvas object globally so main.c can retrieve the surface
	globalThis.__nxjs_surface__ = canvasObj;

	var ctx = createContext2D(canvasObj);

	return {
		canvas: {
			width: width,
			height: height,
			_native: canvasObj,
		},
		ctx: ctx
	};
}

// Expose createCanvas globally
globalThis.createCanvas = createCanvas;
