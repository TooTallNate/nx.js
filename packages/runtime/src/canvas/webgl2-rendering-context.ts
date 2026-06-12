import { $ } from '../$';
import { createInternal, def, proto, stub } from '../utils';
import { ImageData } from './image-data';
import { OffscreenCanvas } from './offscreen-canvas';
import type { Screen } from '../screen';
import type { Image } from '../image';
import type { ImageBitmap } from './image-bitmap';

// Local mirrors of the lib.dom WebGL typedefs. The runtime's public type
// surface must not reference lib.dom (consumers compile with
// no-default-lib; see AGENTS.md), so these are declared here and emitted
// into the bundled dist/index.d.ts.
export type GLenum = number;
export type GLbitfield = number;
export type GLboolean = boolean;
export type GLint = number;
export type GLsizei = number;
export type GLintptr = number;
export type GLsizeiptr = number;
export type GLuint = number;
export type GLfloat = number;
export type GLclampf = number;
export type GLint64 = number;
export type GLuint64 = number;
export type Float32List = Float32Array | number[];
export type Int32List = Int32Array | number[];
export type Uint32List = Uint32Array | number[];

/**
 * Image sources accepted by `texImage2D()` / `texSubImage2D()`.
 */
export type TexImageSource =
	| ImageData
	| ImageBitmap
	| Image
	| Screen
	| OffscreenCanvas;

export interface WebGLContextAttributes {
	alpha?: boolean;
	antialias?: boolean;
	depth?: boolean;
	desynchronized?: boolean;
	failIfMajorPerformanceCaveat?: boolean;
	powerPreference?: 'default' | 'high-performance' | 'low-power';
	premultipliedAlpha?: boolean;
	preserveDrawingBuffer?: boolean;
	stencil?: boolean;
}

const ILLEGAL = () => new TypeError('Illegal constructor');

/**
 * Opaque handle to a WebGL buffer object (`gl.createBuffer()`).
 *
 * @see https://developer.mozilla.org/docs/Web/API/WebGLBuffer
 */
export class WebGLBuffer {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLBuffer);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLFramebuffer */
export class WebGLFramebuffer {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLFramebuffer);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLProgram */
export class WebGLProgram {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLProgram);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLQuery */
export class WebGLQuery {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLQuery);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLRenderbuffer */
export class WebGLRenderbuffer {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLRenderbuffer);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLSampler */
export class WebGLSampler {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLSampler);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLShader */
export class WebGLShader {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLShader);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLSync */
export class WebGLSync {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLSync);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLTexture */
export class WebGLTexture {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLTexture);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLTransformFeedback */
export class WebGLTransformFeedback {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLTransformFeedback);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLUniformLocation */
export class WebGLUniformLocation {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLUniformLocation);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLVertexArrayObject */
export class WebGLVertexArrayObject {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLVertexArrayObject);

/**
 * Information about a uniform or attribute, returned by
 * `gl.getActiveUniform()` / `gl.getActiveAttrib()`.
 *
 * @see https://developer.mozilla.org/docs/Web/API/WebGLActiveInfo
 */
export class WebGLActiveInfo {
	declare readonly name: string;
	declare readonly size: GLint;
	declare readonly type: GLenum;
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLActiveInfo);

/** @see https://developer.mozilla.org/docs/Web/API/WebGLShaderPrecisionFormat */
export class WebGLShaderPrecisionFormat {
	declare readonly precision: GLint;
	declare readonly rangeMax: GLint;
	declare readonly rangeMin: GLint;
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}
}
def(WebGLShaderPrecisionFormat);

interface WebGL2Internal {
	canvas: Screen;
}

const _ = createInternal<WebGL2RenderingContext, WebGL2Internal>();

// All WebGL1 + WebGL2 enum constants (mirrors lib.dom).
const GL_CONSTANTS = {
	DEPTH_BUFFER_BIT: 0x00000100,
	STENCIL_BUFFER_BIT: 0x00000400,
	COLOR_BUFFER_BIT: 0x00004000,
	POINTS: 0x0000,
	LINES: 0x0001,
	LINE_LOOP: 0x0002,
	LINE_STRIP: 0x0003,
	TRIANGLES: 0x0004,
	TRIANGLE_STRIP: 0x0005,
	TRIANGLE_FAN: 0x0006,
	ZERO: 0,
	ONE: 1,
	SRC_COLOR: 0x0300,
	ONE_MINUS_SRC_COLOR: 0x0301,
	SRC_ALPHA: 0x0302,
	ONE_MINUS_SRC_ALPHA: 0x0303,
	DST_ALPHA: 0x0304,
	ONE_MINUS_DST_ALPHA: 0x0305,
	DST_COLOR: 0x0306,
	ONE_MINUS_DST_COLOR: 0x0307,
	SRC_ALPHA_SATURATE: 0x0308,
	FUNC_ADD: 0x8006,
	BLEND_EQUATION: 0x8009,
	BLEND_EQUATION_RGB: 0x8009,
	BLEND_EQUATION_ALPHA: 0x883D,
	FUNC_SUBTRACT: 0x800A,
	FUNC_REVERSE_SUBTRACT: 0x800B,
	BLEND_DST_RGB: 0x80C8,
	BLEND_SRC_RGB: 0x80C9,
	BLEND_DST_ALPHA: 0x80CA,
	BLEND_SRC_ALPHA: 0x80CB,
	CONSTANT_COLOR: 0x8001,
	ONE_MINUS_CONSTANT_COLOR: 0x8002,
	CONSTANT_ALPHA: 0x8003,
	ONE_MINUS_CONSTANT_ALPHA: 0x8004,
	BLEND_COLOR: 0x8005,
	ARRAY_BUFFER: 0x8892,
	ELEMENT_ARRAY_BUFFER: 0x8893,
	ARRAY_BUFFER_BINDING: 0x8894,
	ELEMENT_ARRAY_BUFFER_BINDING: 0x8895,
	STREAM_DRAW: 0x88E0,
	STATIC_DRAW: 0x88E4,
	DYNAMIC_DRAW: 0x88E8,
	BUFFER_SIZE: 0x8764,
	BUFFER_USAGE: 0x8765,
	CURRENT_VERTEX_ATTRIB: 0x8626,
	FRONT: 0x0404,
	BACK: 0x0405,
	FRONT_AND_BACK: 0x0408,
	CULL_FACE: 0x0B44,
	BLEND: 0x0BE2,
	DITHER: 0x0BD0,
	STENCIL_TEST: 0x0B90,
	DEPTH_TEST: 0x0B71,
	SCISSOR_TEST: 0x0C11,
	POLYGON_OFFSET_FILL: 0x8037,
	SAMPLE_ALPHA_TO_COVERAGE: 0x809E,
	SAMPLE_COVERAGE: 0x80A0,
	NO_ERROR: 0,
	INVALID_ENUM: 0x0500,
	INVALID_VALUE: 0x0501,
	INVALID_OPERATION: 0x0502,
	OUT_OF_MEMORY: 0x0505,
	CW: 0x0900,
	CCW: 0x0901,
	LINE_WIDTH: 0x0B21,
	ALIASED_POINT_SIZE_RANGE: 0x846D,
	ALIASED_LINE_WIDTH_RANGE: 0x846E,
	CULL_FACE_MODE: 0x0B45,
	FRONT_FACE: 0x0B46,
	DEPTH_RANGE: 0x0B70,
	DEPTH_WRITEMASK: 0x0B72,
	DEPTH_CLEAR_VALUE: 0x0B73,
	DEPTH_FUNC: 0x0B74,
	STENCIL_CLEAR_VALUE: 0x0B91,
	STENCIL_FUNC: 0x0B92,
	STENCIL_FAIL: 0x0B94,
	STENCIL_PASS_DEPTH_FAIL: 0x0B95,
	STENCIL_PASS_DEPTH_PASS: 0x0B96,
	STENCIL_REF: 0x0B97,
	STENCIL_VALUE_MASK: 0x0B93,
	STENCIL_WRITEMASK: 0x0B98,
	STENCIL_BACK_FUNC: 0x8800,
	STENCIL_BACK_FAIL: 0x8801,
	STENCIL_BACK_PASS_DEPTH_FAIL: 0x8802,
	STENCIL_BACK_PASS_DEPTH_PASS: 0x8803,
	STENCIL_BACK_REF: 0x8CA3,
	STENCIL_BACK_VALUE_MASK: 0x8CA4,
	STENCIL_BACK_WRITEMASK: 0x8CA5,
	VIEWPORT: 0x0BA2,
	SCISSOR_BOX: 0x0C10,
	COLOR_CLEAR_VALUE: 0x0C22,
	COLOR_WRITEMASK: 0x0C23,
	UNPACK_ALIGNMENT: 0x0CF5,
	PACK_ALIGNMENT: 0x0D05,
	MAX_TEXTURE_SIZE: 0x0D33,
	MAX_VIEWPORT_DIMS: 0x0D3A,
	SUBPIXEL_BITS: 0x0D50,
	RED_BITS: 0x0D52,
	GREEN_BITS: 0x0D53,
	BLUE_BITS: 0x0D54,
	ALPHA_BITS: 0x0D55,
	DEPTH_BITS: 0x0D56,
	STENCIL_BITS: 0x0D57,
	POLYGON_OFFSET_UNITS: 0x2A00,
	POLYGON_OFFSET_FACTOR: 0x8038,
	TEXTURE_BINDING_2D: 0x8069,
	SAMPLE_BUFFERS: 0x80A8,
	SAMPLES: 0x80A9,
	SAMPLE_COVERAGE_VALUE: 0x80AA,
	SAMPLE_COVERAGE_INVERT: 0x80AB,
	COMPRESSED_TEXTURE_FORMATS: 0x86A3,
	DONT_CARE: 0x1100,
	FASTEST: 0x1101,
	NICEST: 0x1102,
	GENERATE_MIPMAP_HINT: 0x8192,
	BYTE: 0x1400,
	UNSIGNED_BYTE: 0x1401,
	SHORT: 0x1402,
	UNSIGNED_SHORT: 0x1403,
	INT: 0x1404,
	UNSIGNED_INT: 0x1405,
	FLOAT: 0x1406,
	DEPTH_COMPONENT: 0x1902,
	ALPHA: 0x1906,
	RGB: 0x1907,
	RGBA: 0x1908,
	LUMINANCE: 0x1909,
	LUMINANCE_ALPHA: 0x190A,
	UNSIGNED_SHORT_4_4_4_4: 0x8033,
	UNSIGNED_SHORT_5_5_5_1: 0x8034,
	UNSIGNED_SHORT_5_6_5: 0x8363,
	FRAGMENT_SHADER: 0x8B30,
	VERTEX_SHADER: 0x8B31,
	MAX_VERTEX_ATTRIBS: 0x8869,
	MAX_VERTEX_UNIFORM_VECTORS: 0x8DFB,
	MAX_VARYING_VECTORS: 0x8DFC,
	MAX_COMBINED_TEXTURE_IMAGE_UNITS: 0x8B4D,
	MAX_VERTEX_TEXTURE_IMAGE_UNITS: 0x8B4C,
	MAX_TEXTURE_IMAGE_UNITS: 0x8872,
	MAX_FRAGMENT_UNIFORM_VECTORS: 0x8DFD,
	SHADER_TYPE: 0x8B4F,
	DELETE_STATUS: 0x8B80,
	LINK_STATUS: 0x8B82,
	VALIDATE_STATUS: 0x8B83,
	ATTACHED_SHADERS: 0x8B85,
	ACTIVE_UNIFORMS: 0x8B86,
	ACTIVE_ATTRIBUTES: 0x8B89,
	SHADING_LANGUAGE_VERSION: 0x8B8C,
	CURRENT_PROGRAM: 0x8B8D,
	NEVER: 0x0200,
	LESS: 0x0201,
	EQUAL: 0x0202,
	LEQUAL: 0x0203,
	GREATER: 0x0204,
	NOTEQUAL: 0x0205,
	GEQUAL: 0x0206,
	ALWAYS: 0x0207,
	KEEP: 0x1E00,
	REPLACE: 0x1E01,
	INCR: 0x1E02,
	DECR: 0x1E03,
	INVERT: 0x150A,
	INCR_WRAP: 0x8507,
	DECR_WRAP: 0x8508,
	VENDOR: 0x1F00,
	RENDERER: 0x1F01,
	VERSION: 0x1F02,
	NEAREST: 0x2600,
	LINEAR: 0x2601,
	NEAREST_MIPMAP_NEAREST: 0x2700,
	LINEAR_MIPMAP_NEAREST: 0x2701,
	NEAREST_MIPMAP_LINEAR: 0x2702,
	LINEAR_MIPMAP_LINEAR: 0x2703,
	TEXTURE_MAG_FILTER: 0x2800,
	TEXTURE_MIN_FILTER: 0x2801,
	TEXTURE_WRAP_S: 0x2802,
	TEXTURE_WRAP_T: 0x2803,
	TEXTURE_2D: 0x0DE1,
	TEXTURE: 0x1702,
	TEXTURE_CUBE_MAP: 0x8513,
	TEXTURE_BINDING_CUBE_MAP: 0x8514,
	TEXTURE_CUBE_MAP_POSITIVE_X: 0x8515,
	TEXTURE_CUBE_MAP_NEGATIVE_X: 0x8516,
	TEXTURE_CUBE_MAP_POSITIVE_Y: 0x8517,
	TEXTURE_CUBE_MAP_NEGATIVE_Y: 0x8518,
	TEXTURE_CUBE_MAP_POSITIVE_Z: 0x8519,
	TEXTURE_CUBE_MAP_NEGATIVE_Z: 0x851A,
	MAX_CUBE_MAP_TEXTURE_SIZE: 0x851C,
	TEXTURE0: 0x84C0,
	TEXTURE1: 0x84C1,
	TEXTURE2: 0x84C2,
	TEXTURE3: 0x84C3,
	TEXTURE4: 0x84C4,
	TEXTURE5: 0x84C5,
	TEXTURE6: 0x84C6,
	TEXTURE7: 0x84C7,
	TEXTURE8: 0x84C8,
	TEXTURE9: 0x84C9,
	TEXTURE10: 0x84CA,
	TEXTURE11: 0x84CB,
	TEXTURE12: 0x84CC,
	TEXTURE13: 0x84CD,
	TEXTURE14: 0x84CE,
	TEXTURE15: 0x84CF,
	TEXTURE16: 0x84D0,
	TEXTURE17: 0x84D1,
	TEXTURE18: 0x84D2,
	TEXTURE19: 0x84D3,
	TEXTURE20: 0x84D4,
	TEXTURE21: 0x84D5,
	TEXTURE22: 0x84D6,
	TEXTURE23: 0x84D7,
	TEXTURE24: 0x84D8,
	TEXTURE25: 0x84D9,
	TEXTURE26: 0x84DA,
	TEXTURE27: 0x84DB,
	TEXTURE28: 0x84DC,
	TEXTURE29: 0x84DD,
	TEXTURE30: 0x84DE,
	TEXTURE31: 0x84DF,
	ACTIVE_TEXTURE: 0x84E0,
	REPEAT: 0x2901,
	CLAMP_TO_EDGE: 0x812F,
	MIRRORED_REPEAT: 0x8370,
	FLOAT_VEC2: 0x8B50,
	FLOAT_VEC3: 0x8B51,
	FLOAT_VEC4: 0x8B52,
	INT_VEC2: 0x8B53,
	INT_VEC3: 0x8B54,
	INT_VEC4: 0x8B55,
	BOOL: 0x8B56,
	BOOL_VEC2: 0x8B57,
	BOOL_VEC3: 0x8B58,
	BOOL_VEC4: 0x8B59,
	FLOAT_MAT2: 0x8B5A,
	FLOAT_MAT3: 0x8B5B,
	FLOAT_MAT4: 0x8B5C,
	SAMPLER_2D: 0x8B5E,
	SAMPLER_CUBE: 0x8B60,
	VERTEX_ATTRIB_ARRAY_ENABLED: 0x8622,
	VERTEX_ATTRIB_ARRAY_SIZE: 0x8623,
	VERTEX_ATTRIB_ARRAY_STRIDE: 0x8624,
	VERTEX_ATTRIB_ARRAY_TYPE: 0x8625,
	VERTEX_ATTRIB_ARRAY_NORMALIZED: 0x886A,
	VERTEX_ATTRIB_ARRAY_POINTER: 0x8645,
	VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: 0x889F,
	IMPLEMENTATION_COLOR_READ_TYPE: 0x8B9A,
	IMPLEMENTATION_COLOR_READ_FORMAT: 0x8B9B,
	COMPILE_STATUS: 0x8B81,
	LOW_FLOAT: 0x8DF0,
	MEDIUM_FLOAT: 0x8DF1,
	HIGH_FLOAT: 0x8DF2,
	LOW_INT: 0x8DF3,
	MEDIUM_INT: 0x8DF4,
	HIGH_INT: 0x8DF5,
	FRAMEBUFFER: 0x8D40,
	RENDERBUFFER: 0x8D41,
	RGBA4: 0x8056,
	RGB5_A1: 0x8057,
	RGBA8: 0x8058,
	RGB565: 0x8D62,
	DEPTH_COMPONENT16: 0x81A5,
	STENCIL_INDEX8: 0x8D48,
	DEPTH_STENCIL: 0x84F9,
	RENDERBUFFER_WIDTH: 0x8D42,
	RENDERBUFFER_HEIGHT: 0x8D43,
	RENDERBUFFER_INTERNAL_FORMAT: 0x8D44,
	RENDERBUFFER_RED_SIZE: 0x8D50,
	RENDERBUFFER_GREEN_SIZE: 0x8D51,
	RENDERBUFFER_BLUE_SIZE: 0x8D52,
	RENDERBUFFER_ALPHA_SIZE: 0x8D53,
	RENDERBUFFER_DEPTH_SIZE: 0x8D54,
	RENDERBUFFER_STENCIL_SIZE: 0x8D55,
	FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: 0x8CD0,
	FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: 0x8CD1,
	FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: 0x8CD2,
	FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: 0x8CD3,
	COLOR_ATTACHMENT0: 0x8CE0,
	DEPTH_ATTACHMENT: 0x8D00,
	STENCIL_ATTACHMENT: 0x8D20,
	DEPTH_STENCIL_ATTACHMENT: 0x821A,
	NONE: 0,
	FRAMEBUFFER_COMPLETE: 0x8CD5,
	FRAMEBUFFER_INCOMPLETE_ATTACHMENT: 0x8CD6,
	FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: 0x8CD7,
	FRAMEBUFFER_INCOMPLETE_DIMENSIONS: 0x8CD9,
	FRAMEBUFFER_UNSUPPORTED: 0x8CDD,
	FRAMEBUFFER_BINDING: 0x8CA6,
	RENDERBUFFER_BINDING: 0x8CA7,
	MAX_RENDERBUFFER_SIZE: 0x84E8,
	INVALID_FRAMEBUFFER_OPERATION: 0x0506,
	UNPACK_FLIP_Y_WEBGL: 0x9240,
	UNPACK_PREMULTIPLY_ALPHA_WEBGL: 0x9241,
	CONTEXT_LOST_WEBGL: 0x9242,
	UNPACK_COLORSPACE_CONVERSION_WEBGL: 0x9243,
	BROWSER_DEFAULT_WEBGL: 0x9244,
	READ_BUFFER: 0x0C02,
	UNPACK_ROW_LENGTH: 0x0CF2,
	UNPACK_SKIP_ROWS: 0x0CF3,
	UNPACK_SKIP_PIXELS: 0x0CF4,
	PACK_ROW_LENGTH: 0x0D02,
	PACK_SKIP_ROWS: 0x0D03,
	PACK_SKIP_PIXELS: 0x0D04,
	COLOR: 0x1800,
	DEPTH: 0x1801,
	STENCIL: 0x1802,
	RED: 0x1903,
	RGB8: 0x8051,
	RGB10_A2: 0x8059,
	TEXTURE_BINDING_3D: 0x806A,
	UNPACK_SKIP_IMAGES: 0x806D,
	UNPACK_IMAGE_HEIGHT: 0x806E,
	TEXTURE_3D: 0x806F,
	TEXTURE_WRAP_R: 0x8072,
	MAX_3D_TEXTURE_SIZE: 0x8073,
	UNSIGNED_INT_2_10_10_10_REV: 0x8368,
	MAX_ELEMENTS_VERTICES: 0x80E8,
	MAX_ELEMENTS_INDICES: 0x80E9,
	TEXTURE_MIN_LOD: 0x813A,
	TEXTURE_MAX_LOD: 0x813B,
	TEXTURE_BASE_LEVEL: 0x813C,
	TEXTURE_MAX_LEVEL: 0x813D,
	MIN: 0x8007,
	MAX: 0x8008,
	DEPTH_COMPONENT24: 0x81A6,
	MAX_TEXTURE_LOD_BIAS: 0x84FD,
	TEXTURE_COMPARE_MODE: 0x884C,
	TEXTURE_COMPARE_FUNC: 0x884D,
	CURRENT_QUERY: 0x8865,
	QUERY_RESULT: 0x8866,
	QUERY_RESULT_AVAILABLE: 0x8867,
	STREAM_READ: 0x88E1,
	STREAM_COPY: 0x88E2,
	STATIC_READ: 0x88E5,
	STATIC_COPY: 0x88E6,
	DYNAMIC_READ: 0x88E9,
	DYNAMIC_COPY: 0x88EA,
	MAX_DRAW_BUFFERS: 0x8824,
	DRAW_BUFFER0: 0x8825,
	DRAW_BUFFER1: 0x8826,
	DRAW_BUFFER2: 0x8827,
	DRAW_BUFFER3: 0x8828,
	DRAW_BUFFER4: 0x8829,
	DRAW_BUFFER5: 0x882A,
	DRAW_BUFFER6: 0x882B,
	DRAW_BUFFER7: 0x882C,
	DRAW_BUFFER8: 0x882D,
	DRAW_BUFFER9: 0x882E,
	DRAW_BUFFER10: 0x882F,
	DRAW_BUFFER11: 0x8830,
	DRAW_BUFFER12: 0x8831,
	DRAW_BUFFER13: 0x8832,
	DRAW_BUFFER14: 0x8833,
	DRAW_BUFFER15: 0x8834,
	MAX_FRAGMENT_UNIFORM_COMPONENTS: 0x8B49,
	MAX_VERTEX_UNIFORM_COMPONENTS: 0x8B4A,
	SAMPLER_3D: 0x8B5F,
	SAMPLER_2D_SHADOW: 0x8B62,
	FRAGMENT_SHADER_DERIVATIVE_HINT: 0x8B8B,
	PIXEL_PACK_BUFFER: 0x88EB,
	PIXEL_UNPACK_BUFFER: 0x88EC,
	PIXEL_PACK_BUFFER_BINDING: 0x88ED,
	PIXEL_UNPACK_BUFFER_BINDING: 0x88EF,
	SRGB: 0x8C40,
	SRGB8: 0x8C41,
	SRGB8_ALPHA8: 0x8C43,
	COMPARE_REF_TO_TEXTURE: 0x884E,
	RGBA32F: 0x8814,
	RGB32F: 0x8815,
	RGBA16F: 0x881A,
	RGB16F: 0x881B,
	VERTEX_ATTRIB_ARRAY_INTEGER: 0x88FD,
	MAX_ARRAY_TEXTURE_LAYERS: 0x88FF,
	MIN_PROGRAM_TEXEL_OFFSET: 0x8904,
	MAX_PROGRAM_TEXEL_OFFSET: 0x8905,
	MAX_VARYING_COMPONENTS: 0x8B4B,
	TEXTURE_2D_ARRAY: 0x8C1A,
	TEXTURE_BINDING_2D_ARRAY: 0x8C1D,
	R11F_G11F_B10F: 0x8C3A,
	UNSIGNED_INT_10F_11F_11F_REV: 0x8C3B,
	RGB9_E5: 0x8C3D,
	UNSIGNED_INT_5_9_9_9_REV: 0x8C3E,
	TRANSFORM_FEEDBACK_BUFFER_MODE: 0x8C7F,
	MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS: 0x8C80,
	TRANSFORM_FEEDBACK_VARYINGS: 0x8C83,
	TRANSFORM_FEEDBACK_BUFFER_START: 0x8C84,
	TRANSFORM_FEEDBACK_BUFFER_SIZE: 0x8C85,
	TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN: 0x8C88,
	RASTERIZER_DISCARD: 0x8C89,
	MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: 0x8C8A,
	MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS: 0x8C8B,
	INTERLEAVED_ATTRIBS: 0x8C8C,
	SEPARATE_ATTRIBS: 0x8C8D,
	TRANSFORM_FEEDBACK_BUFFER: 0x8C8E,
	TRANSFORM_FEEDBACK_BUFFER_BINDING: 0x8C8F,
	RGBA32UI: 0x8D70,
	RGB32UI: 0x8D71,
	RGBA16UI: 0x8D76,
	RGB16UI: 0x8D77,
	RGBA8UI: 0x8D7C,
	RGB8UI: 0x8D7D,
	RGBA32I: 0x8D82,
	RGB32I: 0x8D83,
	RGBA16I: 0x8D88,
	RGB16I: 0x8D89,
	RGBA8I: 0x8D8E,
	RGB8I: 0x8D8F,
	RED_INTEGER: 0x8D94,
	RGB_INTEGER: 0x8D98,
	RGBA_INTEGER: 0x8D99,
	SAMPLER_2D_ARRAY: 0x8DC1,
	SAMPLER_2D_ARRAY_SHADOW: 0x8DC4,
	SAMPLER_CUBE_SHADOW: 0x8DC5,
	UNSIGNED_INT_VEC2: 0x8DC6,
	UNSIGNED_INT_VEC3: 0x8DC7,
	UNSIGNED_INT_VEC4: 0x8DC8,
	INT_SAMPLER_2D: 0x8DCA,
	INT_SAMPLER_3D: 0x8DCB,
	INT_SAMPLER_CUBE: 0x8DCC,
	INT_SAMPLER_2D_ARRAY: 0x8DCF,
	UNSIGNED_INT_SAMPLER_2D: 0x8DD2,
	UNSIGNED_INT_SAMPLER_3D: 0x8DD3,
	UNSIGNED_INT_SAMPLER_CUBE: 0x8DD4,
	UNSIGNED_INT_SAMPLER_2D_ARRAY: 0x8DD7,
	DEPTH_COMPONENT32F: 0x8CAC,
	DEPTH32F_STENCIL8: 0x8CAD,
	FLOAT_32_UNSIGNED_INT_24_8_REV: 0x8DAD,
	FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: 0x8210,
	FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: 0x8211,
	FRAMEBUFFER_ATTACHMENT_RED_SIZE: 0x8212,
	FRAMEBUFFER_ATTACHMENT_GREEN_SIZE: 0x8213,
	FRAMEBUFFER_ATTACHMENT_BLUE_SIZE: 0x8214,
	FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE: 0x8215,
	FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE: 0x8216,
	FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE: 0x8217,
	FRAMEBUFFER_DEFAULT: 0x8218,
	UNSIGNED_INT_24_8: 0x84FA,
	DEPTH24_STENCIL8: 0x88F0,
	UNSIGNED_NORMALIZED: 0x8C17,
	DRAW_FRAMEBUFFER_BINDING: 0x8CA6,
	READ_FRAMEBUFFER: 0x8CA8,
	DRAW_FRAMEBUFFER: 0x8CA9,
	READ_FRAMEBUFFER_BINDING: 0x8CAA,
	RENDERBUFFER_SAMPLES: 0x8CAB,
	FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER: 0x8CD4,
	MAX_COLOR_ATTACHMENTS: 0x8CDF,
	COLOR_ATTACHMENT1: 0x8CE1,
	COLOR_ATTACHMENT2: 0x8CE2,
	COLOR_ATTACHMENT3: 0x8CE3,
	COLOR_ATTACHMENT4: 0x8CE4,
	COLOR_ATTACHMENT5: 0x8CE5,
	COLOR_ATTACHMENT6: 0x8CE6,
	COLOR_ATTACHMENT7: 0x8CE7,
	COLOR_ATTACHMENT8: 0x8CE8,
	COLOR_ATTACHMENT9: 0x8CE9,
	COLOR_ATTACHMENT10: 0x8CEA,
	COLOR_ATTACHMENT11: 0x8CEB,
	COLOR_ATTACHMENT12: 0x8CEC,
	COLOR_ATTACHMENT13: 0x8CED,
	COLOR_ATTACHMENT14: 0x8CEE,
	COLOR_ATTACHMENT15: 0x8CEF,
	FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: 0x8D56,
	MAX_SAMPLES: 0x8D57,
	HALF_FLOAT: 0x140B,
	RG: 0x8227,
	RG_INTEGER: 0x8228,
	R8: 0x8229,
	RG8: 0x822B,
	R16F: 0x822D,
	R32F: 0x822E,
	RG16F: 0x822F,
	RG32F: 0x8230,
	R8I: 0x8231,
	R8UI: 0x8232,
	R16I: 0x8233,
	R16UI: 0x8234,
	R32I: 0x8235,
	R32UI: 0x8236,
	RG8I: 0x8237,
	RG8UI: 0x8238,
	RG16I: 0x8239,
	RG16UI: 0x823A,
	RG32I: 0x823B,
	RG32UI: 0x823C,
	VERTEX_ARRAY_BINDING: 0x85B5,
	R8_SNORM: 0x8F94,
	RG8_SNORM: 0x8F95,
	RGB8_SNORM: 0x8F96,
	RGBA8_SNORM: 0x8F97,
	SIGNED_NORMALIZED: 0x8F9C,
	COPY_READ_BUFFER: 0x8F36,
	COPY_WRITE_BUFFER: 0x8F37,
	COPY_READ_BUFFER_BINDING: 0x8F36,
	COPY_WRITE_BUFFER_BINDING: 0x8F37,
	UNIFORM_BUFFER: 0x8A11,
	UNIFORM_BUFFER_BINDING: 0x8A28,
	UNIFORM_BUFFER_START: 0x8A29,
	UNIFORM_BUFFER_SIZE: 0x8A2A,
	MAX_VERTEX_UNIFORM_BLOCKS: 0x8A2B,
	MAX_FRAGMENT_UNIFORM_BLOCKS: 0x8A2D,
	MAX_COMBINED_UNIFORM_BLOCKS: 0x8A2E,
	MAX_UNIFORM_BUFFER_BINDINGS: 0x8A2F,
	MAX_UNIFORM_BLOCK_SIZE: 0x8A30,
	MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS: 0x8A31,
	MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: 0x8A33,
	UNIFORM_BUFFER_OFFSET_ALIGNMENT: 0x8A34,
	ACTIVE_UNIFORM_BLOCKS: 0x8A36,
	UNIFORM_TYPE: 0x8A37,
	UNIFORM_SIZE: 0x8A38,
	UNIFORM_BLOCK_INDEX: 0x8A3A,
	UNIFORM_OFFSET: 0x8A3B,
	UNIFORM_ARRAY_STRIDE: 0x8A3C,
	UNIFORM_MATRIX_STRIDE: 0x8A3D,
	UNIFORM_IS_ROW_MAJOR: 0x8A3E,
	UNIFORM_BLOCK_BINDING: 0x8A3F,
	UNIFORM_BLOCK_DATA_SIZE: 0x8A40,
	UNIFORM_BLOCK_ACTIVE_UNIFORMS: 0x8A42,
	UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: 0x8A43,
	UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER: 0x8A44,
	UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: 0x8A46,
	INVALID_INDEX: 0xFFFFFFFF,
	MAX_VERTEX_OUTPUT_COMPONENTS: 0x9122,
	MAX_FRAGMENT_INPUT_COMPONENTS: 0x9125,
	MAX_SERVER_WAIT_TIMEOUT: 0x9111,
	OBJECT_TYPE: 0x9112,
	SYNC_CONDITION: 0x9113,
	SYNC_STATUS: 0x9114,
	SYNC_FLAGS: 0x9115,
	SYNC_FENCE: 0x9116,
	SYNC_GPU_COMMANDS_COMPLETE: 0x9117,
	UNSIGNALED: 0x9118,
	SIGNALED: 0x9119,
	ALREADY_SIGNALED: 0x911A,
	TIMEOUT_EXPIRED: 0x911B,
	CONDITION_SATISFIED: 0x911C,
	WAIT_FAILED: 0x911D,
	SYNC_FLUSH_COMMANDS_BIT: 0x00000001,
	VERTEX_ATTRIB_ARRAY_DIVISOR: 0x88FE,
	ANY_SAMPLES_PASSED: 0x8C2F,
	ANY_SAMPLES_PASSED_CONSERVATIVE: 0x8D6A,
	SAMPLER_BINDING: 0x8919,
	RGB10_A2UI: 0x906F,
	INT_2_10_10_10_REV: 0x8D9F,
	TRANSFORM_FEEDBACK: 0x8E22,
	TRANSFORM_FEEDBACK_PAUSED: 0x8E23,
	TRANSFORM_FEEDBACK_ACTIVE: 0x8E24,
	TRANSFORM_FEEDBACK_BINDING: 0x8E25,
	TEXTURE_IMMUTABLE_FORMAT: 0x912F,
	MAX_ELEMENT_INDEX: 0x8D6B,
	TEXTURE_IMMUTABLE_LEVELS: 0x82DF,
	MAX_CLIENT_WAIT_TIMEOUT_WEBGL: 0x9247,
};

/**
 * The WebGL2 rendering context for the {@link Screen | `screen`} canvas,
 * backed by a real OpenGL ES 3 context on the Switch GPU. Acquire it with
 * `screen.getContext('webgl2')`.
 *
 * The screen may only have ONE context kind: once a `'2d'` context exists,
 * `getContext('webgl2')` returns `null` (and vice versa).
 *
 * @see https://developer.mozilla.org/docs/Web/API/WebGL2RenderingContext
 */
export class WebGL2RenderingContext {
	/** @ignore */
	constructor() {
		throw ILLEGAL();
	}

	/**
	 * The {@link Screen | `screen`} canvas this context draws to.
	 */
	get canvas(): Screen {
		return _(this).canvas;
	}

	/** The width of the drawing buffer in pixels. */
	declare readonly drawingBufferWidth: GLsizei;

	/** The height of the drawing buffer in pixels. */
	declare readonly drawingBufferHeight: GLsizei;

	/**
	 * Returns the actual context parameters.
	 */
	getContextAttributes(): WebGLContextAttributes {
		return {
			alpha: true,
			antialias: false,
			depth: true,
			desynchronized: false,
			failIfMajorPerformanceCaveat: false,
			powerPreference: 'default',
			premultipliedAlpha: true,
			preserveDrawingBuffer: false,
			stencil: true,
		};
	}

	/**
	 * Returns `false` — the nx.js GL context is never lost.
	 */
	isContextLost(): boolean {
		return false;
	}

	/**
	 * WebGL extensions are not currently implemented; returns an empty array.
	 * The WebGL2 core API (which includes most WebGL1 extension
	 * functionality) is fully available.
	 */
	getSupportedExtensions(): string[] {
		return [];
	}

	/**
	 * WebGL extensions are not currently implemented; returns `null`.
	 */
	getExtension(name: string): any {
		return null;
	}

	activeTexture(texture: GLenum): void {
		stub();
	}
	attachShader(program: WebGLProgram, shader: WebGLShader): void {
		stub();
	}
	bindAttribLocation(program: WebGLProgram, index: GLuint, name: string): void {
		stub();
	}
	bindBuffer(target: GLenum, buffer: WebGLBuffer | null): void {
		stub();
	}
	bindFramebuffer(target: GLenum, framebuffer: WebGLFramebuffer | null): void {
		stub();
	}
	bindRenderbuffer(target: GLenum, renderbuffer: WebGLRenderbuffer | null): void {
		stub();
	}
	bindTexture(target: GLenum, texture: WebGLTexture | null): void {
		stub();
	}
	blendColor(red: GLclampf, green: GLclampf, blue: GLclampf, alpha: GLclampf): void {
		stub();
	}
	blendEquation(mode: GLenum): void {
		stub();
	}
	blendEquationSeparate(modeRGB: GLenum, modeAlpha: GLenum): void {
		stub();
	}
	blendFunc(sfactor: GLenum, dfactor: GLenum): void {
		stub();
	}
	blendFuncSeparate(srcRGB: GLenum, dstRGB: GLenum, srcAlpha: GLenum, dstAlpha: GLenum): void {
		stub();
	}
	checkFramebufferStatus(target: GLenum): GLenum {
		stub();
	}
	clear(mask: GLbitfield): void {
		stub();
	}
	clearColor(red: GLclampf, green: GLclampf, blue: GLclampf, alpha: GLclampf): void {
		stub();
	}
	clearDepth(depth: GLclampf): void {
		stub();
	}
	clearStencil(s: GLint): void {
		stub();
	}
	colorMask(red: GLboolean, green: GLboolean, blue: GLboolean, alpha: GLboolean): void {
		stub();
	}
	compileShader(shader: WebGLShader): void {
		stub();
	}
	copyTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, x: GLint, y: GLint, width: GLsizei, height: GLsizei, border: GLint): void {
		stub();
	}
	copyTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, x: GLint, y: GLint, width: GLsizei, height: GLsizei): void {
		stub();
	}
	createBuffer(): WebGLBuffer | null {
		stub();
	}
	createFramebuffer(): WebGLFramebuffer | null {
		stub();
	}
	createProgram(): WebGLProgram | null {
		stub();
	}
	createRenderbuffer(): WebGLRenderbuffer | null {
		stub();
	}
	createShader(type: GLenum): WebGLShader | null {
		stub();
	}
	createTexture(): WebGLTexture | null {
		stub();
	}
	cullFace(mode: GLenum): void {
		stub();
	}
	deleteBuffer(buffer: WebGLBuffer | null): void {
		stub();
	}
	deleteFramebuffer(framebuffer: WebGLFramebuffer | null): void {
		stub();
	}
	deleteProgram(program: WebGLProgram | null): void {
		stub();
	}
	deleteRenderbuffer(renderbuffer: WebGLRenderbuffer | null): void {
		stub();
	}
	deleteShader(shader: WebGLShader | null): void {
		stub();
	}
	deleteTexture(texture: WebGLTexture | null): void {
		stub();
	}
	depthFunc(func: GLenum): void {
		stub();
	}
	depthMask(flag: GLboolean): void {
		stub();
	}
	depthRange(zNear: GLclampf, zFar: GLclampf): void {
		stub();
	}
	detachShader(program: WebGLProgram, shader: WebGLShader): void {
		stub();
	}
	disable(cap: GLenum): void {
		stub();
	}
	disableVertexAttribArray(index: GLuint): void {
		stub();
	}
	drawArrays(mode: GLenum, first: GLint, count: GLsizei): void {
		stub();
	}
	drawElements(mode: GLenum, count: GLsizei, type: GLenum, offset: GLintptr): void {
		stub();
	}
	enable(cap: GLenum): void {
		stub();
	}
	enableVertexAttribArray(index: GLuint): void {
		stub();
	}
	finish(): void {
		stub();
	}
	flush(): void {
		stub();
	}
	framebufferRenderbuffer(target: GLenum, attachment: GLenum, renderbuffertarget: GLenum, renderbuffer: WebGLRenderbuffer | null): void {
		stub();
	}
	framebufferTexture2D(target: GLenum, attachment: GLenum, textarget: GLenum, texture: WebGLTexture | null, level: GLint): void {
		stub();
	}
	frontFace(mode: GLenum): void {
		stub();
	}
	generateMipmap(target: GLenum): void {
		stub();
	}
	getActiveAttrib(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null {
		stub();
	}
	getActiveUniform(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null {
		stub();
	}
	getAttachedShaders(program: WebGLProgram): WebGLShader[] | null {
		stub();
	}
	getAttribLocation(program: WebGLProgram, name: string): GLint {
		stub();
	}
	getBufferParameter(target: GLenum, pname: GLenum): any {
		stub();
	}
	getError(): GLenum {
		stub();
	}
	getFramebufferAttachmentParameter(target: GLenum, attachment: GLenum, pname: GLenum): any {
		stub();
	}
	getParameter(pname: GLenum): any {
		stub();
	}
	getProgramInfoLog(program: WebGLProgram): string | null {
		stub();
	}
	getProgramParameter(program: WebGLProgram, pname: GLenum): any {
		stub();
	}
	getRenderbufferParameter(target: GLenum, pname: GLenum): any {
		stub();
	}
	getShaderInfoLog(shader: WebGLShader): string | null {
		stub();
	}
	getShaderParameter(shader: WebGLShader, pname: GLenum): any {
		stub();
	}
	getShaderPrecisionFormat(shadertype: GLenum, precisiontype: GLenum): WebGLShaderPrecisionFormat | null {
		stub();
	}
	getShaderSource(shader: WebGLShader): string | null {
		stub();
	}
	getTexParameter(target: GLenum, pname: GLenum): any {
		stub();
	}
	getUniform(program: WebGLProgram, location: WebGLUniformLocation): any {
		stub();
	}
	getUniformLocation(program: WebGLProgram, name: string): WebGLUniformLocation | null {
		stub();
	}
	getVertexAttrib(index: GLuint, pname: GLenum): any {
		stub();
	}
	getVertexAttribOffset(index: GLuint, pname: GLenum): GLintptr {
		stub();
	}
	hint(target: GLenum, mode: GLenum): void {
		stub();
	}
	isBuffer(buffer: WebGLBuffer | null): GLboolean {
		stub();
	}
	isEnabled(cap: GLenum): GLboolean {
		stub();
	}
	isFramebuffer(framebuffer: WebGLFramebuffer | null): GLboolean {
		stub();
	}
	isProgram(program: WebGLProgram | null): GLboolean {
		stub();
	}
	isRenderbuffer(renderbuffer: WebGLRenderbuffer | null): GLboolean {
		stub();
	}
	isShader(shader: WebGLShader | null): GLboolean {
		stub();
	}
	isTexture(texture: WebGLTexture | null): GLboolean {
		stub();
	}
	lineWidth(width: GLfloat): void {
		stub();
	}
	linkProgram(program: WebGLProgram): void {
		stub();
	}
	pixelStorei(pname: GLenum, param: GLint | GLboolean): void {
		stub();
	}
	polygonOffset(factor: GLfloat, units: GLfloat): void {
		stub();
	}
	renderbufferStorage(target: GLenum, internalformat: GLenum, width: GLsizei, height: GLsizei): void {
		stub();
	}
	sampleCoverage(value: GLclampf, invert: GLboolean): void {
		stub();
	}
	scissor(x: GLint, y: GLint, width: GLsizei, height: GLsizei): void {
		stub();
	}
	shaderSource(shader: WebGLShader, source: string): void {
		stub();
	}
	stencilFunc(func: GLenum, ref: GLint, mask: GLuint): void {
		stub();
	}
	stencilFuncSeparate(face: GLenum, func: GLenum, ref: GLint, mask: GLuint): void {
		stub();
	}
	stencilMask(mask: GLuint): void {
		stub();
	}
	stencilMaskSeparate(face: GLenum, mask: GLuint): void {
		stub();
	}
	stencilOp(fail: GLenum, zfail: GLenum, zpass: GLenum): void {
		stub();
	}
	stencilOpSeparate(face: GLenum, fail: GLenum, zfail: GLenum, zpass: GLenum): void {
		stub();
	}
	texParameterf(target: GLenum, pname: GLenum, param: GLfloat): void {
		stub();
	}
	texParameteri(target: GLenum, pname: GLenum, param: GLint): void {
		stub();
	}
	uniform1f(location: WebGLUniformLocation | null, x: GLfloat): void {
		stub();
	}
	uniform1i(location: WebGLUniformLocation | null, x: GLint): void {
		stub();
	}
	uniform2f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat): void {
		stub();
	}
	uniform2i(location: WebGLUniformLocation | null, x: GLint, y: GLint): void {
		stub();
	}
	uniform3f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat, z: GLfloat): void {
		stub();
	}
	uniform3i(location: WebGLUniformLocation | null, x: GLint, y: GLint, z: GLint): void {
		stub();
	}
	uniform4f(location: WebGLUniformLocation | null, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat): void {
		stub();
	}
	uniform4i(location: WebGLUniformLocation | null, x: GLint, y: GLint, z: GLint, w: GLint): void {
		stub();
	}
	useProgram(program: WebGLProgram | null): void {
		stub();
	}
	validateProgram(program: WebGLProgram): void {
		stub();
	}
	vertexAttrib1f(index: GLuint, x: GLfloat): void {
		stub();
	}
	vertexAttrib1fv(index: GLuint, values: Float32List): void {
		stub();
	}
	vertexAttrib2f(index: GLuint, x: GLfloat, y: GLfloat): void {
		stub();
	}
	vertexAttrib2fv(index: GLuint, values: Float32List): void {
		stub();
	}
	vertexAttrib3f(index: GLuint, x: GLfloat, y: GLfloat, z: GLfloat): void {
		stub();
	}
	vertexAttrib3fv(index: GLuint, values: Float32List): void {
		stub();
	}
	vertexAttrib4f(index: GLuint, x: GLfloat, y: GLfloat, z: GLfloat, w: GLfloat): void {
		stub();
	}
	vertexAttrib4fv(index: GLuint, values: Float32List): void {
		stub();
	}
	vertexAttribPointer(index: GLuint, size: GLint, type: GLenum, normalized: GLboolean, stride: GLsizei, offset: GLintptr): void {
		stub();
	}
	viewport(x: GLint, y: GLint, width: GLsizei, height: GLsizei): void {
		stub();
	}
	bufferData(target: GLenum, size: GLsizeiptr, usage: GLenum): void;
	bufferData(target: GLenum, data: BufferSource | null, usage: GLenum): void;
	bufferData(target: GLenum, srcData: BufferSource | null, usage: GLenum): void;
	bufferData(target: GLenum, srcData: ArrayBufferView, usage: GLenum, srcOffset: number, length?: GLuint): void;
	bufferData(...args: unknown[]): any {
		stub();
	}
	bufferSubData(target: GLenum, offset: GLintptr, data: BufferSource): void;
	bufferSubData(target: GLenum, dstByteOffset: GLintptr, srcData: BufferSource): void;
	bufferSubData(target: GLenum, dstByteOffset: GLintptr, srcData: ArrayBufferView, srcOffset: number, length?: GLuint): void;
	bufferSubData(...args: unknown[]): any {
		stub();
	}
	compressedTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, data: ArrayBufferView): void;
	compressedTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, imageSize: GLsizei, offset: GLintptr): void;
	compressedTexImage2D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, border: GLint, srcData: ArrayBufferView, srcOffset?: number, srcLengthOverride?: GLuint): void;
	compressedTexImage2D(...args: unknown[]): any {
		stub();
	}
	compressedTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, data: ArrayBufferView): void;
	compressedTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, imageSize: GLsizei, offset: GLintptr): void;
	compressedTexSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, srcData: ArrayBufferView, srcOffset?: number, srcLengthOverride?: GLuint): void;
	compressedTexSubImage2D(...args: unknown[]): any {
		stub();
	}
	readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
	readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, dstData: ArrayBufferView | null): void;
	readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, offset: GLintptr): void;
	readPixels(x: GLint, y: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, dstData: ArrayBufferView, dstOffset: number): void;
	readPixels(...args: unknown[]): any {
		stub();
	}
	texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
	texImage2D(target: GLenum, level: GLint, internalformat: GLint, format: GLenum, type: GLenum, source: TexImageSource): void;
	texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
	texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, source: TexImageSource): void;
	texImage2D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, border: GLint, format: GLenum, type: GLenum, srcData: ArrayBufferView, srcOffset: number): void;
	texImage2D(...args: unknown[]): any {
		stub();
	}
	texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pixels: ArrayBufferView | null): void;
	texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, format: GLenum, type: GLenum, source: TexImageSource): void;
	texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
	texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, source: TexImageSource): void;
	texSubImage2D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, width: GLsizei, height: GLsizei, format: GLenum, type: GLenum, srcData: ArrayBufferView, srcOffset: number): void;
	texSubImage2D(...args: unknown[]): any {
		stub();
	}
	uniform1fv(location: WebGLUniformLocation | null, v: Float32List): void;
	uniform1fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform1fv(...args: unknown[]): any {
		stub();
	}
	uniform1iv(location: WebGLUniformLocation | null, v: Int32List): void;
	uniform1iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform1iv(...args: unknown[]): any {
		stub();
	}
	uniform2fv(location: WebGLUniformLocation | null, v: Float32List): void;
	uniform2fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform2fv(...args: unknown[]): any {
		stub();
	}
	uniform2iv(location: WebGLUniformLocation | null, v: Int32List): void;
	uniform2iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform2iv(...args: unknown[]): any {
		stub();
	}
	uniform3fv(location: WebGLUniformLocation | null, v: Float32List): void;
	uniform3fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform3fv(...args: unknown[]): any {
		stub();
	}
	uniform3iv(location: WebGLUniformLocation | null, v: Int32List): void;
	uniform3iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform3iv(...args: unknown[]): any {
		stub();
	}
	uniform4fv(location: WebGLUniformLocation | null, v: Float32List): void;
	uniform4fv(location: WebGLUniformLocation | null, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform4fv(...args: unknown[]): any {
		stub();
	}
	uniform4iv(location: WebGLUniformLocation | null, v: Int32List): void;
	uniform4iv(location: WebGLUniformLocation | null, data: Int32List, srcOffset?: number, srcLength?: GLuint): void;
	uniform4iv(...args: unknown[]): any {
		stub();
	}
	uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, value: Float32List): void;
	uniformMatrix2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniformMatrix2fv(...args: unknown[]): any {
		stub();
	}
	uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, value: Float32List): void;
	uniformMatrix3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniformMatrix3fv(...args: unknown[]): any {
		stub();
	}
	uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, value: Float32List): void;
	uniformMatrix4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void;
	uniformMatrix4fv(...args: unknown[]): any {
		stub();
	}
	beginQuery(target: GLenum, query: WebGLQuery): void {
		stub();
	}
	beginTransformFeedback(primitiveMode: GLenum): void {
		stub();
	}
	bindBufferBase(target: GLenum, index: GLuint, buffer: WebGLBuffer | null): void {
		stub();
	}
	bindBufferRange(target: GLenum, index: GLuint, buffer: WebGLBuffer | null, offset: GLintptr, size: GLsizeiptr): void {
		stub();
	}
	bindSampler(unit: GLuint, sampler: WebGLSampler | null): void {
		stub();
	}
	bindTransformFeedback(target: GLenum, tf: WebGLTransformFeedback | null): void {
		stub();
	}
	bindVertexArray(array: WebGLVertexArrayObject | null): void {
		stub();
	}
	blitFramebuffer(srcX0: GLint, srcY0: GLint, srcX1: GLint, srcY1: GLint, dstX0: GLint, dstY0: GLint, dstX1: GLint, dstY1: GLint, mask: GLbitfield, filter: GLenum): void {
		stub();
	}
	clearBufferfi(buffer: GLenum, drawbuffer: GLint, depth: GLfloat, stencil: GLint): void {
		stub();
	}
	clearBufferfv(buffer: GLenum, drawbuffer: GLint, values: Float32List, srcOffset?: number): void {
		stub();
	}
	clearBufferiv(buffer: GLenum, drawbuffer: GLint, values: Int32List, srcOffset?: number): void {
		stub();
	}
	clearBufferuiv(buffer: GLenum, drawbuffer: GLint, values: Uint32List, srcOffset?: number): void {
		stub();
	}
	clientWaitSync(sync: WebGLSync, flags: GLbitfield, timeout: GLuint64): GLenum {
		stub();
	}
	compressedTexImage3D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, imageSize: GLsizei, offset: GLintptr): void;
	compressedTexImage3D(target: GLenum, level: GLint, internalformat: GLenum, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, srcData: ArrayBufferView, srcOffset?: number, srcLengthOverride?: GLuint): void;
	compressedTexImage3D(...args: unknown[]): any {
		stub();
	}
	compressedTexSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, imageSize: GLsizei, offset: GLintptr): void;
	compressedTexSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, srcData: ArrayBufferView, srcOffset?: number, srcLengthOverride?: GLuint): void;
	compressedTexSubImage3D(...args: unknown[]): any {
		stub();
	}
	copyBufferSubData(readTarget: GLenum, writeTarget: GLenum, readOffset: GLintptr, writeOffset: GLintptr, size: GLsizeiptr): void {
		stub();
	}
	copyTexSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, x: GLint, y: GLint, width: GLsizei, height: GLsizei): void {
		stub();
	}
	createQuery(): WebGLQuery | null {
		stub();
	}
	createSampler(): WebGLSampler | null {
		stub();
	}
	createTransformFeedback(): WebGLTransformFeedback | null {
		stub();
	}
	createVertexArray(): WebGLVertexArrayObject | null {
		stub();
	}
	deleteQuery(query: WebGLQuery | null): void {
		stub();
	}
	deleteSampler(sampler: WebGLSampler | null): void {
		stub();
	}
	deleteSync(sync: WebGLSync | null): void {
		stub();
	}
	deleteTransformFeedback(tf: WebGLTransformFeedback | null): void {
		stub();
	}
	deleteVertexArray(vertexArray: WebGLVertexArrayObject | null): void {
		stub();
	}
	drawArraysInstanced(mode: GLenum, first: GLint, count: GLsizei, instanceCount: GLsizei): void {
		stub();
	}
	drawBuffers(buffers: GLenum[]): void {
		stub();
	}
	drawElementsInstanced(mode: GLenum, count: GLsizei, type: GLenum, offset: GLintptr, instanceCount: GLsizei): void {
		stub();
	}
	drawRangeElements(mode: GLenum, start: GLuint, end: GLuint, count: GLsizei, type: GLenum, offset: GLintptr): void {
		stub();
	}
	endQuery(target: GLenum): void {
		stub();
	}
	endTransformFeedback(): void {
		stub();
	}
	fenceSync(condition: GLenum, flags: GLbitfield): WebGLSync | null {
		stub();
	}
	framebufferTextureLayer(target: GLenum, attachment: GLenum, texture: WebGLTexture | null, level: GLint, layer: GLint): void {
		stub();
	}
	getActiveUniformBlockName(program: WebGLProgram, uniformBlockIndex: GLuint): string | null {
		stub();
	}
	getActiveUniformBlockParameter(program: WebGLProgram, uniformBlockIndex: GLuint, pname: GLenum): any {
		stub();
	}
	getActiveUniforms(program: WebGLProgram, uniformIndices: GLuint[], pname: GLenum): any {
		stub();
	}
	getBufferSubData(target: GLenum, srcByteOffset: GLintptr, dstBuffer: ArrayBufferView, dstOffset?: number, length?: GLuint): void {
		stub();
	}
	getFragDataLocation(program: WebGLProgram, name: string): GLint {
		stub();
	}
	getIndexedParameter(target: GLenum, index: GLuint): any {
		stub();
	}
	getInternalformatParameter(target: GLenum, internalformat: GLenum, pname: GLenum): any {
		stub();
	}
	getQuery(target: GLenum, pname: GLenum): WebGLQuery | null {
		stub();
	}
	getQueryParameter(query: WebGLQuery, pname: GLenum): any {
		stub();
	}
	getSamplerParameter(sampler: WebGLSampler, pname: GLenum): any {
		stub();
	}
	getSyncParameter(sync: WebGLSync, pname: GLenum): any {
		stub();
	}
	getTransformFeedbackVarying(program: WebGLProgram, index: GLuint): WebGLActiveInfo | null {
		stub();
	}
	getUniformBlockIndex(program: WebGLProgram, uniformBlockName: string): GLuint {
		stub();
	}
	getUniformIndices(program: WebGLProgram, uniformNames: string[]): GLuint[] | null {
		stub();
	}
	invalidateFramebuffer(target: GLenum, attachments: GLenum[]): void {
		stub();
	}
	invalidateSubFramebuffer(target: GLenum, attachments: GLenum[], x: GLint, y: GLint, width: GLsizei, height: GLsizei): void {
		stub();
	}
	isQuery(query: WebGLQuery | null): GLboolean {
		stub();
	}
	isSampler(sampler: WebGLSampler | null): GLboolean {
		stub();
	}
	isSync(sync: WebGLSync | null): GLboolean {
		stub();
	}
	isTransformFeedback(tf: WebGLTransformFeedback | null): GLboolean {
		stub();
	}
	isVertexArray(vertexArray: WebGLVertexArrayObject | null): GLboolean {
		stub();
	}
	pauseTransformFeedback(): void {
		stub();
	}
	readBuffer(src: GLenum): void {
		stub();
	}
	renderbufferStorageMultisample(target: GLenum, samples: GLsizei, internalformat: GLenum, width: GLsizei, height: GLsizei): void {
		stub();
	}
	resumeTransformFeedback(): void {
		stub();
	}
	samplerParameterf(sampler: WebGLSampler, pname: GLenum, param: GLfloat): void {
		stub();
	}
	samplerParameteri(sampler: WebGLSampler, pname: GLenum, param: GLint): void {
		stub();
	}
	texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
	texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, source: TexImageSource): void;
	texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, srcData: ArrayBufferView | null): void;
	texImage3D(target: GLenum, level: GLint, internalformat: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, border: GLint, format: GLenum, type: GLenum, srcData: ArrayBufferView, srcOffset: number): void;
	texImage3D(...args: unknown[]): any {
		stub();
	}
	texStorage2D(target: GLenum, levels: GLsizei, internalformat: GLenum, width: GLsizei, height: GLsizei): void {
		stub();
	}
	texStorage3D(target: GLenum, levels: GLsizei, internalformat: GLenum, width: GLsizei, height: GLsizei, depth: GLsizei): void {
		stub();
	}
	texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, pboOffset: GLintptr): void;
	texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, source: TexImageSource): void;
	texSubImage3D(target: GLenum, level: GLint, xoffset: GLint, yoffset: GLint, zoffset: GLint, width: GLsizei, height: GLsizei, depth: GLsizei, format: GLenum, type: GLenum, srcData: ArrayBufferView | null, srcOffset?: number): void;
	texSubImage3D(...args: unknown[]): any {
		stub();
	}
	transformFeedbackVaryings(program: WebGLProgram, varyings: string[], bufferMode: GLenum): void {
		stub();
	}
	uniform1ui(location: WebGLUniformLocation | null, v0: GLuint): void {
		stub();
	}
	uniform1uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniform2ui(location: WebGLUniformLocation | null, v0: GLuint, v1: GLuint): void {
		stub();
	}
	uniform2uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniform3ui(location: WebGLUniformLocation | null, v0: GLuint, v1: GLuint, v2: GLuint): void {
		stub();
	}
	uniform3uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniform4ui(location: WebGLUniformLocation | null, v0: GLuint, v1: GLuint, v2: GLuint, v3: GLuint): void {
		stub();
	}
	uniform4uiv(location: WebGLUniformLocation | null, data: Uint32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniformBlockBinding(program: WebGLProgram, uniformBlockIndex: GLuint, uniformBlockBinding: GLuint): void {
		stub();
	}
	uniformMatrix2x3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniformMatrix2x4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniformMatrix3x2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniformMatrix3x4fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniformMatrix4x2fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	uniformMatrix4x3fv(location: WebGLUniformLocation | null, transpose: GLboolean, data: Float32List, srcOffset?: number, srcLength?: GLuint): void {
		stub();
	}
	vertexAttribDivisor(index: GLuint, divisor: GLuint): void {
		stub();
	}
	vertexAttribI4i(index: GLuint, x: GLint, y: GLint, z: GLint, w: GLint): void {
		stub();
	}
	vertexAttribI4iv(index: GLuint, values: Int32List): void {
		stub();
	}
	vertexAttribI4ui(index: GLuint, x: GLuint, y: GLuint, z: GLuint, w: GLuint): void {
		stub();
	}
	vertexAttribI4uiv(index: GLuint, values: Uint32List): void {
		stub();
	}
	vertexAttribIPointer(index: GLuint, size: GLint, type: GLenum, stride: GLsizei, offset: GLintptr): void {
		stub();
	}
	waitSync(sync: WebGLSync, flags: GLbitfield, timeout: GLint64): void {
		stub();
	}
}

// The numeric GL constants exist on both the class and every instance,
// matching browser behavior.
export interface WebGL2RenderingContext extends Readonly<typeof GL_CONSTANTS> {}

for (const [k, v] of Object.entries(GL_CONSTANTS)) {
	Object.defineProperty(WebGL2RenderingContext, k, { value: v });
	Object.defineProperty(WebGL2RenderingContext.prototype, k, { value: v });
}
def(WebGL2RenderingContext);

$.webglInitClass(WebGL2RenderingContext, {
	WebGLBuffer,
	WebGLFramebuffer,
	WebGLProgram,
	WebGLQuery,
	WebGLRenderbuffer,
	WebGLSampler,
	WebGLShader,
	WebGLSync,
	WebGLTexture,
	WebGLTransformFeedback,
	WebGLUniformLocation,
	WebGLVertexArrayObject,
	WebGLActiveInfo,
	WebGLShaderPrecisionFormat,
});

// ---------------------------------------------------------------------------
// TexImageSource normalization: the native texImage2D/texSubImage2D bindings
// only handle ArrayBufferView / PBO-offset pixel data. Wrap them so the
// TexImageSource overloads (ImageData / Image / ImageBitmap / canvases) are
// converted to raw RGBA bytes first. (Must run AFTER $.webglInitClass, which
// installs the native methods on the prototype.)
// ---------------------------------------------------------------------------

function isTexImageSource(v: any): boolean {
	return (
		v !== null &&
		typeof v === 'object' &&
		!ArrayBuffer.isView(v) &&
		typeof v.width === 'number' &&
		typeof v.height === 'number'
	);
}

function sourceToPixels(src: any): {
	data: Uint8Array;
	width: number;
	height: number;
} {
	let id: ImageData;
	if (src instanceof ImageData) {
		id = src;
	} else {
		// Rasterize through an offscreen 2D canvas; getImageData() returns
		// non-premultiplied RGBA, which is exactly what GL expects with the
		// default UNPACK_PREMULTIPLY_ALPHA_WEBGL = false.
		const c = new OffscreenCanvas(src.width, src.height);
		const ctx = c.getContext('2d');
		ctx.drawImage(src, 0, 0);
		id = ctx.getImageData(0, 0, src.width, src.height) as ImageData;
	}
	return {
		data: new Uint8Array(
			id.data.buffer,
			id.data.byteOffset,
			id.data.byteLength,
		),
		width: id.width,
		height: id.height,
	};
}

{
	const p: any = WebGL2RenderingContext.prototype;
	const nativeTexImage2D = p.texImage2D;
	const nativeTexSubImage2D = p.texSubImage2D;
	if (typeof nativeTexImage2D === 'function') {
		p.texImage2D = function (...args: any[]) {
			const last = args[args.length - 1];
			if (isTexImageSource(last)) {
				const px = sourceToPixels(last);
				if (args.length === 6) {
					// (target, level, internalformat, format, type, source)
					args = [
						args[0],
						args[1],
						args[2],
						px.width,
						px.height,
						0,
						args[3],
						args[4],
						px.data,
					];
				} else {
					// (..., width, height, border, format, type, source)
					args[args.length - 1] = px.data;
				}
			}
			return nativeTexImage2D.apply(this, args);
		};
		p.texSubImage2D = function (...args: any[]) {
			const last = args[args.length - 1];
			if (isTexImageSource(last)) {
				const px = sourceToPixels(last);
				if (args.length === 7) {
					// (target, level, xoffset, yoffset, format, type, source)
					args = [
						args[0],
						args[1],
						args[2],
						args[3],
						px.width,
						px.height,
						args[4],
						args[5],
						px.data,
					];
				} else {
					// (..., width, height, format, type, source)
					args[args.length - 1] = px.data;
				}
			}
			return nativeTexSubImage2D.apply(this, args);
		};
	}
}

/**
 * @ignore
 * Internal factory used by `Screen#getContext('webgl2')`. Returns `null`
 * when the GL context could not be created (e.g. EGL/ES3 init failure).
 */
export function createWebGL2Context(
	canvas: Screen,
): WebGL2RenderingContext | null {
	const c = $.webglContextNew(canvas);
	if (!c) return null;
	const ctx = proto(c, WebGL2RenderingContext);
	_.set(ctx, { canvas });
	return ctx;
}
