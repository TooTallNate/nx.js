/**
 * DOM shim for Phaser 3 on nx.js
 *
 * nx.js has no DOM — only `screen`, `OffscreenCanvas`, `Image`, `requestAnimationFrame`,
 * and basic event support. This shim provides the minimum surface Phaser needs to boot
 * its Canvas renderer.
 */

const SCREEN_W = 1280;
const SCREEN_H = 720;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Create a Proxy that silently returns noops / empty values for unknown props */
function lenientProxy<T extends object>(target: T, name = 'proxy'): T {
	return new Proxy(target, {
		get(t, prop, receiver) {
			if (prop in t) return Reflect.get(t, prop, receiver);
			// Common property patterns Phaser probes
			if (typeof prop === 'symbol') return undefined;
			// Return a noop function for method calls, undefined for props
			return undefined;
		},
		set(t, prop, value) {
			(t as any)[prop] = value;
			return true;
		},
	});
}

function makeStyleProxy(): any {
	return new Proxy(
		{},
		{
			get: () => '',
			set: () => true,
		},
	);
}

// ---------------------------------------------------------------------------
// Patch OffscreenCanvas instances to look like HTMLCanvasElement
// ---------------------------------------------------------------------------

/** Safely set a property, skipping if it's read-only (getter without setter) */
function safeProp(obj: any, key: string, value: any) {
	const desc = Object.getOwnPropertyDescriptor(obj, key) ??
		Object.getOwnPropertyDescriptor(Object.getPrototypeOf(obj) ?? {}, key);
	if (desc && desc.get && !desc.set) return; // read-only getter, skip
	try { obj[key] = value; } catch {}
}

function patchCanvas(c: OffscreenCanvas): any {
	const obj = c as any;
	if (!obj.__patched) {
		obj.__patched = true;
		safeProp(obj, 'style', makeStyleProxy());
		safeProp(obj, 'parentNode', null);
		safeProp(obj, 'parentElement', null);
		safeProp(obj, 'classList', { add() {}, remove() {}, contains() { return false; } });

		const attrs: Record<string, string> = {};
		safeProp(obj, 'setAttribute', (k: string, v: string) => { attrs[k] = v; });
		safeProp(obj, 'getAttribute', (k: string) => attrs[k] ?? null);
		safeProp(obj, 'removeAttribute', () => {});
		safeProp(obj, 'hasAttribute', (k: string) => k in attrs);

		safeProp(obj, 'getBoundingClientRect', () => ({
			x: 0, y: 0, top: 0, left: 0,
			width: c.width, height: c.height,
			right: c.width, bottom: c.height,
			toJSON() {},
		}));

		safeProp(obj, 'focus', () => {});
		safeProp(obj, 'blur', () => {});

		// Save original addEventListener/removeEventListener if present (e.g. on screen)
		const origAddEventListener = obj.addEventListener?.bind(obj);
		const origRemoveEventListener = obj.removeEventListener?.bind(obj);

		obj.addEventListener = (type: string, fn: any, opts?: any) => {
			// Forward to the original if it exists (screen has real touch events)
			if (origAddEventListener) {
				try { origAddEventListener(type, fn, opts); } catch {}
			}
			// Also forward to window for Phaser's global event handling
			try { (globalThis as any).addEventListener(type, fn, opts); } catch {}
		};
		obj.removeEventListener = (type: string, fn: any, opts?: any) => {
			if (origRemoveEventListener) {
				try { origRemoveEventListener(type, fn, opts); } catch {}
			}
			try { (globalThis as any).removeEventListener(type, fn, opts); } catch {}
		};
		safeProp(obj, 'dispatchEvent', (e: Event) => (globalThis as any).dispatchEvent(e));

		// Phaser checks these — only set if not already defined as getters
		safeProp(obj, 'nodeName', 'CANVAS');
		safeProp(obj, 'tagName', 'CANVAS');
		safeProp(obj, 'nodeType', 1);

		// Phaser's CanvasPool.remove calls .parentNode.removeChild
		// Already null parentNode so that path is skipped.

		// getContext patching – ensure returned ctx has .canvas ref and style
		const origGetContext = c.getContext.bind(c);
		obj.getContext = (type: string, attrs?: any) => {
			const ctx = origGetContext(type as any, attrs);
			if (ctx) {
				safeProp(ctx, 'canvas', obj);
				// Ensure imageSmoothingEnabled exists
				if (!('imageSmoothingEnabled' in (ctx as any))) {
					safeProp(ctx, 'imageSmoothingEnabled', true);
				}
			}
			return ctx;
		};
	}
	return obj;
}

// Patch screen.orientation before Phaser accesses it (nx.js throws "not implemented")
try {
	Object.defineProperty(screen, 'orientation', {
		value: {
			type: 'landscape-primary',
			angle: 0,
			addEventListener() {},
			removeEventListener() {},
			dispatchEvent() { return false; },
			lock() { return Promise.resolve(); },
			unlock() {},
		},
		configurable: true,
	});
} catch {}

// Patch the global `screen` canvas too
patchCanvas(screen as any);

// ---------------------------------------------------------------------------
// HTMLCanvasElement / HTMLElement stubs
// ---------------------------------------------------------------------------

(globalThis as any).HTMLCanvasElement = class HTMLCanvasElement {};
(globalThis as any).HTMLElement = class HTMLElement {};
(globalThis as any).HTMLVideoElement = class HTMLVideoElement {};
(globalThis as any).HTMLDivElement = class HTMLDivElement {};
(globalThis as any).CSSStyleDeclaration = class CSSStyleDeclaration {};
(globalThis as any).Element = class Element {};
(globalThis as any).Node = class Node {};
(globalThis as any).DOMRect = class DOMRect {
	x = 0; y = 0; width = 0; height = 0;
	top = 0; left = 0; right = 0; bottom = 0;
};

// Make screen instanceof HTMLCanvasElement return true (sort-of)
// Phaser uses `instanceof` checks — we override via Symbol.hasInstance
Object.defineProperty((globalThis as any).HTMLCanvasElement, Symbol.hasInstance, {
	value: (instance: any) => {
		return instance === screen || (instance && instance.getContext && instance.tagName === 'CANVAS');
	},
});

// ---------------------------------------------------------------------------
// document
// ---------------------------------------------------------------------------

const bodyStub: any = lenientProxy({
	appendChild: () => {},
	removeChild: () => {},
	insertBefore: () => {},
	contains: () => false,
	getBoundingClientRect: () => ({
		x: 0, y: 0, top: 0, left: 0,
		width: SCREEN_W, height: SCREEN_H,
		right: SCREEN_W, bottom: SCREEN_H,
		toJSON() {},
	}),
	style: makeStyleProxy(),
	classList: { add() {}, remove() {}, contains() { return false; } },
	nodeName: 'BODY',
	tagName: 'BODY',
	nodeType: 1,
	parentNode: null,
	parentElement: null,
	childNodes: [],
	children: [],
	offsetWidth: SCREEN_W,
	offsetHeight: SCREEN_H,
	scrollLeft: 0,
	scrollTop: 0,
	clientWidth: SCREEN_W,
	clientHeight: SCREEN_H,
});

const documentElementStub: any = lenientProxy({
	clientWidth: SCREEN_W,
	clientHeight: SCREEN_H,
	style: makeStyleProxy(),
	nodeName: 'HTML',
	tagName: 'HTML',
	nodeType: 1,
});

const headStub: any = {
	appendChild: () => {},
	removeChild: () => {},
	querySelectorAll: () => [],
};

const doc: any = {
	readyState: 'complete',
	documentElement: documentElementStub,
	head: headStub,
	body: bodyStub,
	fullscreenElement: null,
	pointerLockElement: null,
	visibilityState: 'visible',
	hidden: false,

	createElement(tag: string) {
		if (tag === 'canvas') {
			const c = new OffscreenCanvas(1, 1);
			return patchCanvas(c);
		}
		// Return a stub element for anything else (div, etc.)
		return lenientProxy({
			style: makeStyleProxy(),
			appendChild: () => {},
			removeChild: () => {},
			insertBefore: () => {},
			setAttribute: () => {},
			getAttribute: () => null,
			removeAttribute: () => {},
			addEventListener: () => {},
			removeEventListener: () => {},
			getBoundingClientRect: () => ({
				x: 0, y: 0, top: 0, left: 0,
				width: 0, height: 0,
				right: 0, bottom: 0, toJSON() {},
			}),
			classList: { add() {}, remove() {}, contains() { return false; } },
			nodeName: tag.toUpperCase(),
			tagName: tag.toUpperCase(),
			nodeType: 1,
			parentNode: null,
			childNodes: [],
			children: [],
			innerHTML: '',
			textContent: '',
		}, `element<${tag}>`);
	},

	createElementNS(_ns: string, tag: string) {
		return doc.createElement(tag);
	},

	getElementById(id: string) {
		if (id === 'game') return patchCanvas(screen as any);
		return null;
	},

	querySelector() { return null; },
	querySelectorAll() { return []; },

	addEventListener(type: string, fn: any, opts?: any) {
		// Forward visibility/fullscreen events – mostly noops
		if (type === 'visibilitychange' || type === 'fullscreenchange') return;
		(globalThis as any).addEventListener(type, fn, opts);
	},
	removeEventListener(type: string, fn: any, opts?: any) {
		(globalThis as any).removeEventListener(type, fn, opts);
	},

	createTextNode(text: string) {
		return { nodeType: 3, textContent: text };
	},

	createDocumentFragment() {
		return {
			appendChild: () => {},
			childNodes: [],
		};
	},

	hasFocus() { return true; },

	exitFullscreen() { return Promise.resolve(); },
	exitPointerLock() {},
};

(globalThis as any).document = doc;

// ---------------------------------------------------------------------------
// window properties Phaser expects
// ---------------------------------------------------------------------------

(globalThis as any).innerWidth = SCREEN_W;
(globalThis as any).innerHeight = SCREEN_H;
(globalThis as any).outerWidth = SCREEN_W;
(globalThis as any).outerHeight = SCREEN_H;
(globalThis as any).screenLeft = 0;
(globalThis as any).screenTop = 0;
(globalThis as any).scrollX = 0;
(globalThis as any).scrollY = 0;
(globalThis as any).pageXOffset = 0;
(globalThis as any).pageYOffset = 0;
(globalThis as any).devicePixelRatio = 1;

if (!(globalThis as any).focus) (globalThis as any).focus = () => {};
if (!(globalThis as any).blur) (globalThis as any).blur = () => {};
if (!(globalThis as any).close) (globalThis as any).close = () => {};
if (!(globalThis as any).getComputedStyle) {
	(globalThis as any).getComputedStyle = () => makeStyleProxy();
}
if (!(globalThis as any).matchMedia) {
	(globalThis as any).matchMedia = () => ({
		matches: false,
		addListener: () => {},
		removeListener: () => {},
		addEventListener: () => {},
		removeEventListener: () => {},
	});
}
if (!(globalThis as any).URL) {
	(globalThis as any).URL = class URL { constructor(public href: string) {} toString() { return this.href; } };
}

// navigator.userAgent – Phaser feature-detects via UA
if (typeof navigator !== 'undefined') {
	try {
		Object.defineProperty(navigator, 'userAgent', {
			value: 'Mozilla/5.0 (Linux; nx.js) AppleWebKit/537.36 Chrome/120.0.0.0',
			configurable: true,
			writable: true,
			enumerable: true,
		});
	} catch {
		// If defineProperty fails, try direct assignment
		try { (navigator as any).userAgent = 'Mozilla/5.0 (Linux; nx.js) AppleWebKit/537.36 Chrome/120.0.0.0'; } catch {}
	}
}

// Phaser uses window.XMLHttpRequest for loader – stub if missing
if (!(globalThis as any).XMLHttpRequest) {
	(globalThis as any).XMLHttpRequest = class XMLHttpRequest {
		open() {}
		send() {}
		setRequestHeader() {}
		addEventListener() {}
		removeEventListener() {}
		abort() {}
		status = 0;
		readyState = 0;
		response = null;
		responseText = '';
		responseType = '';
		onload: any = null;
		onerror: any = null;
		onprogress: any = null;
		onreadystatechange: any = null;
	};
}

// Phaser uses window.CanvasRenderingContext2D for checks
if (!(globalThis as any).CanvasRenderingContext2D) {
	(globalThis as any).CanvasRenderingContext2D = class CanvasRenderingContext2D {};
}
if (!(globalThis as any).WebGLRenderingContext) {
	(globalThis as any).WebGLRenderingContext = class WebGLRenderingContext {};
}

// Export patchCanvas for use in main
export { patchCanvas, SCREEN_W, SCREEN_H };
