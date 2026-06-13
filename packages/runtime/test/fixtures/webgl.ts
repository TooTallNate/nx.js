import { test } from '../src/tap';

// WebGL2 class-surface conformance. The actual GL context only exists on
// Switch hardware (EGL/Mesa) — `getContext('webgl2')` returns null on the
// host harness — so this fixture only asserts the parts that are
// environment-independent: the classes, their constants, and constructor
// behavior. These hold in Chrome too, so it runs in both targets.

declare const WebGL2RenderingContext: any;
declare const WebGLBuffer: any;
declare const WebGLTexture: any;
declare const WebGLProgram: any;
declare const WebGLShader: any;
declare const WebGLFramebuffer: any;
declare const WebGLRenderbuffer: any;
declare const WebGLVertexArrayObject: any;
declare const WebGLUniformLocation: any;
declare const WebGLActiveInfo: any;
declare const WebGLSync: any;
declare const WebGLSampler: any;
declare const WebGLQuery: any;
declare const WebGLTransformFeedback: any;

test('WebGL2RenderingContext class surface', (t) => {
	t.equal(
		typeof WebGL2RenderingContext,
		'function',
		'WebGL2RenderingContext is a global class',
	);
	t.throws(
		() => new WebGL2RenderingContext(),
		'Illegal constructor',
		'constructor throws Illegal constructor',
	);

	// Constants exist on both the class (static) and the prototype.
	t.equal(WebGL2RenderingContext.TRIANGLES, 4, 'static TRIANGLES');
	t.equal(
		WebGL2RenderingContext.prototype.TRIANGLES,
		4,
		'prototype TRIANGLES',
	);
	t.equal(
		WebGL2RenderingContext.DEPTH_BUFFER_BIT,
		0x0100,
		'DEPTH_BUFFER_BIT',
	);
	t.equal(
		WebGL2RenderingContext.COLOR_BUFFER_BIT,
		0x4000,
		'COLOR_BUFFER_BIT',
	);
	t.equal(WebGL2RenderingContext.ARRAY_BUFFER, 0x8892, 'ARRAY_BUFFER');
	t.equal(
		WebGL2RenderingContext.ELEMENT_ARRAY_BUFFER,
		0x8893,
		'ELEMENT_ARRAY_BUFFER',
	);
	t.equal(WebGL2RenderingContext.TEXTURE_2D, 0x0de1, 'TEXTURE_2D');
	t.equal(WebGL2RenderingContext.FRAGMENT_SHADER, 0x8b30, 'FRAGMENT_SHADER');
	t.equal(WebGL2RenderingContext.VERTEX_SHADER, 0x8b31, 'VERTEX_SHADER');
	t.equal(WebGL2RenderingContext.FLOAT, 0x1406, 'FLOAT');
	t.equal(WebGL2RenderingContext.UNSIGNED_BYTE, 0x1401, 'UNSIGNED_BYTE');
	t.equal(WebGL2RenderingContext.RGBA, 0x1908, 'RGBA');
	// WebGL2-only constants
	t.equal(WebGL2RenderingContext.RGBA8, 0x8058, 'RGBA8 (WebGL2)');
	t.equal(
		WebGL2RenderingContext.UNIFORM_BUFFER,
		0x8a11,
		'UNIFORM_BUFFER (WebGL2)',
	);
	t.equal(
		WebGL2RenderingContext.TRANSFORM_FEEDBACK,
		0x8e22,
		'TRANSFORM_FEEDBACK (WebGL2)',
	);
	t.equal(
		WebGL2RenderingContext.PIXEL_UNPACK_BUFFER,
		0x88ec,
		'PIXEL_UNPACK_BUFFER (WebGL2)',
	);
	// WebGL-specific (non-GL) constants
	t.equal(
		WebGL2RenderingContext.UNPACK_FLIP_Y_WEBGL,
		0x9240,
		'UNPACK_FLIP_Y_WEBGL',
	);
	t.equal(
		WebGL2RenderingContext.UNPACK_PREMULTIPLY_ALPHA_WEBGL,
		0x9241,
		'UNPACK_PREMULTIPLY_ALPHA_WEBGL',
	);
});

test('WebGL object classes', (t) => {
	const classes: [string, any][] = [
		['WebGLBuffer', WebGLBuffer],
		['WebGLFramebuffer', WebGLFramebuffer],
		['WebGLProgram', WebGLProgram],
		['WebGLQuery', WebGLQuery],
		['WebGLRenderbuffer', WebGLRenderbuffer],
		['WebGLSampler', WebGLSampler],
		['WebGLShader', WebGLShader],
		['WebGLSync', WebGLSync],
		['WebGLTexture', WebGLTexture],
		['WebGLTransformFeedback', WebGLTransformFeedback],
		['WebGLUniformLocation', WebGLUniformLocation],
		['WebGLVertexArrayObject', WebGLVertexArrayObject],
		['WebGLActiveInfo', WebGLActiveInfo],
	];
	for (const [name, cls] of classes) {
		t.equal(typeof cls, 'function', `${name} is a global class`);
	}
	t.throws(
		() => new WebGLBuffer(),
		'Illegal constructor',
		'new WebGLBuffer() throws',
	);
	t.throws(
		() => new WebGLUniformLocation(),
		'Illegal constructor',
		'new WebGLUniformLocation() throws',
	);
});
