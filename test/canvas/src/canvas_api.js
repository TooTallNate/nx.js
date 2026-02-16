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
	// Create the native context via C
	var nativeCtx = $.canvasContext2dNew(canvasObj);

	// The nativeCtx is the opaque object that holds the C state.
	// C functions expect it as the first argument (via argv[0] / CANVAS_CONTEXT_ARGV0)
	// or as 'this' (via CANVAS_CONTEXT_THIS).
	// Looking at the C code, canvasContext2dSetFillStyle expects (this_unused, argc=5, argv=[context, r, g, b, a])
	// Actually, looking more carefully:
	//   init_function_list entries are plain JS_CFUNC_DEF, so they're called as $.func(args...)
	//   But canvasContext2dInitClass sets up methods on the prototype with NX_DEF_FUNC
	//   which means fillRect etc are methods on nativeCtx itself.

	// The prototype methods (fillRect, etc.) are set directly on the Context2DClass prototype
	// by canvasContext2dInitClass. So nativeCtx already has those methods.
	// But fillStyle/strokeStyle are handled through the canvasContext2dSetFillStyle/
	// canvasContext2dGetFillStyle global functions which take (this, context, r, g, b, a).

	// Actually, looking at the C code for canvasContext2dSetFillStyle:
	//   CANVAS_CONTEXT_ARGV0 — gets context from argv[0]
	//   then reads args from argv[1..4]
	// So it's called as: $.canvasContext2dSetFillStyle(nativeCtx, r, g, b, a)

	// The prototype methods use CANVAS_CONTEXT_THIS — gets context from this_val
	// So they're called as: nativeCtx.fillRect(x, y, w, h)

	var wrapper = {
		_native: nativeCtx,
		_fillStyle: '#000000',
		_strokeStyle: '#000000',

		// ---- Style properties ----
		get fillStyle() {
			return this._fillStyle;
		},
		set fillStyle(v) {
			var parsed = parseColor(v);
			if (!parsed) return;
			this._fillStyle = v;
			$.canvasContext2dSetFillStyle(this._native, parsed[0], parsed[1], parsed[2], parsed[3]);
		},

		get strokeStyle() {
			return this._strokeStyle;
		},
		set strokeStyle(v) {
			var parsed = parseColor(v);
			if (!parsed) return;
			this._strokeStyle = v;
			$.canvasContext2dSetStrokeStyle(this._native, parsed[0], parsed[1], parsed[2], parsed[3]);
		},

		// ---- Global alpha ----
		get globalAlpha() {
			return this._native.globalAlpha;
		},
		set globalAlpha(v) {
			this._native.globalAlpha = v;
		},

		// ---- Global composite operation ----
		get globalCompositeOperation() {
			return this._native.globalCompositeOperation;
		},
		set globalCompositeOperation(v) {
			this._native.globalCompositeOperation = v;
		},

		// ---- Line styles ----
		get lineWidth() {
			return this._native.lineWidth;
		},
		set lineWidth(v) {
			this._native.lineWidth = v;
		},

		get lineCap() {
			return this._native.lineCap;
		},
		set lineCap(v) {
			this._native.lineCap = v;
		},

		get lineJoin() {
			return this._native.lineJoin;
		},
		set lineJoin(v) {
			this._native.lineJoin = v;
		},

		get miterLimit() {
			return this._native.miterLimit;
		},
		set miterLimit(v) {
			this._native.miterLimit = v;
		},

		get lineDashOffset() {
			return this._native.lineDashOffset;
		},
		set lineDashOffset(v) {
			this._native.lineDashOffset = v;
		},

		// ---- Image smoothing ----
		get imageSmoothingEnabled() {
			return this._native.imageSmoothingEnabled;
		},
		set imageSmoothingEnabled(v) {
			this._native.imageSmoothingEnabled = v;
		},

		// ---- Drawing methods ----
		fillRect: function(x, y, w, h) {
			return this._native.fillRect(x, y, w, h);
		},
		strokeRect: function(x, y, w, h) {
			return this._native.strokeRect(x, y, w, h);
		},
		clearRect: function(x, y, w, h) {
			return this._native.clearRect(x, y, w, h);
		},

		// ---- Path methods ----
		beginPath: function() {
			return this._native.beginPath();
		},
		closePath: function() {
			return this._native.closePath();
		},
		moveTo: function(x, y) {
			return this._native.moveTo(x, y);
		},
		lineTo: function(x, y) {
			return this._native.lineTo(x, y);
		},
		arc: function(x, y, r, startAngle, endAngle, ccw) {
			return this._native.arc(x, y, r, startAngle, endAngle, ccw || false);
		},
		arcTo: function(x1, y1, x2, y2, r) {
			return this._native.arcTo(x1, y1, x2, y2, r);
		},
		bezierCurveTo: function(cp1x, cp1y, cp2x, cp2y, x, y) {
			return this._native.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
		},
		quadraticCurveTo: function(cpx, cpy, x, y) {
			return this._native.quadraticCurveTo(cpx, cpy, x, y);
		},
		rect: function(x, y, w, h) {
			return this._native.rect(x, y, w, h);
		},
		roundRect: function(x, y, w, h, radii) {
			if (typeof radii === 'number') {
				return this._native.roundRect(x, y, w, h, radii);
			} else if (Array.isArray(radii)) {
				return this._native.roundRect(x, y, w, h, radii);
			}
			return this._native.roundRect(x, y, w, h, 0);
		},
		ellipse: function(x, y, rx, ry, rotation, startAngle, endAngle, ccw) {
			return this._native.ellipse(x, y, rx, ry, rotation, startAngle, endAngle, ccw || false);
		},
		fill: function(fillRule) {
			return this._native.fill(fillRule);
		},
		stroke: function() {
			return this._native.stroke();
		},
		clip: function(fillRule) {
			return this._native.clip(fillRule);
		},

		// ---- Transform methods ----
		translate: function(x, y) {
			return this._native.translate(x, y);
		},
		rotate: function(angle) {
			return this._native.rotate(angle);
		},
		scale: function(x, y) {
			return this._native.scale(x, y);
		},
		transform: function(a, b, c, d, e, f) {
			return this._native.transform(a, b, c, d, e, f);
		},
		setTransform: function(a, b, c, d, e, f) {
			if (arguments.length === 0) {
				return this._native.resetTransform();
			}
			return this._native.setTransform(a, b, c, d, e, f);
		},
		resetTransform: function() {
			return this._native.resetTransform();
		},

		// ---- Line dash ----
		setLineDash: function(segments) {
			return this._native.setLineDash(segments);
		},
		getLineDash: function() {
			return this._native.getLineDash();
		},

		// ---- State ----
		save: function() {
			return this._native.save();
		},
		restore: function() {
			return this._native.restore();
		},
	};

	return wrapper;
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
