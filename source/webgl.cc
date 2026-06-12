/**
 * WebGL2 — V8 bindings over a real OpenGL ES 3 context (EGL + Mesa/nouveau).
 *
 * The screen is the only canvas that can have a WebGL2 context. The app
 * renders into the EGL window surface's default framebuffer (FBO 0) and the
 * main loop presents with eglSwapBuffers (see nx_webgl_present). There is a
 * single module-global context (`st`); the JS context object is just a
 * prototype carrier — every binding operates on the current GL context.
 *
 * Design notes:
 *   - WebGL object types (WebGLBuffer, WebGLTexture, ...) are wrapped objects
 *     holding the GL name. The TS side passes the JS classes to
 *     $.webglInitClass so freshly-minted objects get the right prototype
 *     (instanceof works). GC finalizers free only the wrapper struct — GL
 *     resources are freed by the explicit delete*() calls (or context exit).
 *   - WebGL-only pixel store parameters (UNPACK_FLIP_Y_WEBGL,
 *     UNPACK_PREMULTIPLY_ALPHA_WEBGL) are emulated at texImage2D/texSubImage2D
 *     upload time for ArrayBufferView sources.
 *   - A synthetic error slot backs WebGL-level validation failures (e.g.
 *     too-small ArrayBufferView for an upload); getError() drains it before
 *     glGetError().
 *   - Uploads/readbacks are size-validated against the source/destination
 *     view so a short buffer can't make the driver read/write out of bounds.
 */
#include "webgl.h"
#include "error.h"
#include "wrap.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include <string>
#include <vector>

using namespace v8;

// WebGL-specific enums (not part of GLES headers).
#define NX_GL_UNPACK_FLIP_Y_WEBGL 0x9240
#define NX_GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL 0x9241
#define NX_GL_UNPACK_COLORSPACE_CONVERSION_WEBGL 0x9243

namespace {

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

enum ObjKind : uint8_t {
	K_BUFFER,
	K_FRAMEBUFFER,
	K_PROGRAM,
	K_QUERY,
	K_RENDERBUFFER,
	K_SAMPLER,
	K_SHADER,
	K_TEXTURE,
	K_TRANSFORM_FEEDBACK,
	K_VERTEX_ARRAY,
	K_SYNC,
	K_UNIFORM_LOCATION,
	K_ACTIVE_INFO,
	K_SHADER_PRECISION_FORMAT,
	K_COUNT,
};

struct GLObj {
	uint32_t id;
	GLsync sync;
	int32_t loc; // uniform location (K_UNIFORM_LOCATION)
	uint8_t kind;
};

struct WebGLState {
	EGLDisplay dpy = EGL_NO_DISPLAY;
	EGLSurface surf = EGL_NO_SURFACE;
	EGLContext ctx = EGL_NO_CONTEXT;
	int width = 0;
	int height = 0;
	bool active = false;
	// The default framebuffer was drawn to since the last present.
	bool dirty = false;
	GLenum synthetic_error = GL_NO_ERROR;
	// WebGL-only pixel store emulation state.
	bool unpack_flip_y = false;
	bool unpack_premultiply = false;
	int unpack_alignment = 4;
	int unpack_row_length = 0;
	int pack_alignment = 4;
	// Currently bound DRAW_FRAMEBUFFER GL name (0 = default/back buffer).
	uint32_t draw_fbo = 0;
	// Prototypes for the WebGL object classes (set by $.webglInitClass).
	Global<Object> protos[K_COUNT];
};

WebGLState *st = nullptr;

void free_gl_obj(GLObj *o) { delete o; }

Local<Object> new_gl_obj(Isolate *iso, uint8_t kind, GLuint id,
                         GLsync sync = nullptr, GLint loc = -1) {
	Local<Object> obj = nx::NewWrapped(iso);
	if (st && !st->protos[kind].IsEmpty()) {
		obj->SetPrototype(iso->GetCurrentContext(), st->protos[kind].Get(iso))
		    .Check();
	}
	GLObj *o = new GLObj{id, sync, loc, kind};
	nx::Wrap<GLObj>(iso, obj, o, free_gl_obj);
	return obj;
}

GLObj *get_gl_obj(Local<Value> v) {
	if (v.IsEmpty() || !v->IsObject())
		return nullptr;
	return nx::Unwrap<GLObj>(v);
}

// GL name of a WebGL object argument (0 for null/undefined, matching GL's
// "no object" semantics).
GLuint obj_id(Local<Value> v) {
	GLObj *o = get_gl_obj(v);
	return o ? o->id : 0;
}

void record_error(GLenum err) {
	if (st && st->synthetic_error == GL_NO_ERROR)
		st->synthetic_error = err;
}

// Mark the default framebuffer dirty (present needed) when it is the current
// draw target.
inline void mark_dirty() {
	if (st && st->draw_fbo == 0)
		st->dirty = true;
}

// ---------------------------------------------------------------------------
// Argument helpers
// ---------------------------------------------------------------------------

inline Local<Context> cur(Isolate *iso) { return iso->GetCurrentContext(); }

inline uint32_t a_u32(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->Uint32Value(cur(info.GetIsolate())).FromMaybe(0);
}
inline int32_t a_i32(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->Int32Value(cur(info.GetIsolate())).FromMaybe(0);
}
inline double a_f64(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->NumberValue(cur(info.GetIsolate())).FromMaybe(0.0);
}
inline float a_f32(const FunctionCallbackInfo<Value> &info, int i) {
	return (float)a_f64(info, i);
}
inline bool a_bool(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->BooleanValue(info.GetIsolate());
}
inline int64_t a_i64(const FunctionCallbackInfo<Value> &info, int i) {
	return info[i]->IntegerValue(cur(info.GetIsolate())).FromMaybe(0);
}

// Raw bytes of an ArrayBufferView (honoring byteOffset). Returns nullptr when
// not a view. elem_size receives the per-element byte width (1 for DataView).
uint8_t *view_bytes(Local<Value> v, size_t *len, size_t *elem_size = nullptr) {
	if (v.IsEmpty() || !v->IsArrayBufferView()) {
		if (len)
			*len = 0;
		return nullptr;
	}
	Local<ArrayBufferView> view = v.As<ArrayBufferView>();
	if (len)
		*len = view->ByteLength();
	if (elem_size) {
		*elem_size = 1;
		if (v->IsTypedArray()) {
			Local<TypedArray> ta = v.As<TypedArray>();
			size_t n = ta->Length();
			if (n > 0)
				*elem_size = ta->ByteLength() / n;
		}
	}
	return (uint8_t *)view->Buffer()->Data() + view->ByteOffset();
}

// Numeric list helper: extracts a typed pointer + element count from either a
// matching TypedArray (zero-copy) or a plain JS array (copied into `tmp`).
// srcOffset/srcLength are in elements (WebGL2 overloads); 0/0 = whole list.
template <typename T, typename TA>
bool get_list(Isolate *iso, Local<Value> v, bool is_ta, std::vector<T> &tmp,
              const T **ptr, size_t *count, uint32_t srcOffset = 0,
              uint32_t srcLength = 0) {
	if (!v.IsEmpty() && is_ta) {
		Local<TA> ta = v.As<TA>();
		size_t n = ta->Length();
		if (srcOffset > n) {
			record_error(GL_INVALID_VALUE);
			return false;
		}
		size_t avail = n - srcOffset;
		size_t want = srcLength ? srcLength : avail;
		if (want > avail) {
			record_error(GL_INVALID_VALUE);
			return false;
		}
		*ptr = (const T *)((uint8_t *)ta->Buffer()->Data() + ta->ByteOffset()) +
		       srcOffset;
		*count = want;
		return true;
	}
	if (!v.IsEmpty() && v->IsArray()) {
		Local<Array> arr = v.As<Array>();
		Local<Context> ctx = cur(iso);
		uint32_t n = arr->Length();
		if (srcOffset > n) {
			record_error(GL_INVALID_VALUE);
			return false;
		}
		uint32_t want = srcLength ? srcLength : (n - srcOffset);
		if (srcOffset + want > n) {
			record_error(GL_INVALID_VALUE);
			return false;
		}
		tmp.resize(want);
		for (uint32_t i = 0; i < want; i++) {
			Local<Value> el;
			double d = 0;
			if (arr->Get(ctx, srcOffset + i).ToLocal(&el))
				d = el->NumberValue(ctx).FromMaybe(0.0);
			tmp[i] = (T)d;
		}
		*ptr = tmp.data();
		*count = want;
		return true;
	}
	record_error(GL_INVALID_VALUE);
	return false;
}

inline bool get_f32_list(Isolate *iso, Local<Value> v, std::vector<float> &tmp,
                         const float **ptr, size_t *count,
                         uint32_t srcOffset = 0, uint32_t srcLength = 0) {
	return get_list<float, Float32Array>(iso, v,
	                                     !v.IsEmpty() && v->IsFloat32Array(),
	                                     tmp, ptr, count, srcOffset, srcLength);
}
inline bool get_i32_list(Isolate *iso, Local<Value> v, std::vector<int32_t> &tmp,
                         const int32_t **ptr, size_t *count,
                         uint32_t srcOffset = 0, uint32_t srcLength = 0) {
	return get_list<int32_t, Int32Array>(iso, v,
	                                     !v.IsEmpty() && v->IsInt32Array(),
	                                     tmp, ptr, count, srcOffset, srcLength);
}
inline bool get_u32_list(Isolate *iso, Local<Value> v,
                         std::vector<uint32_t> &tmp, const uint32_t **ptr,
                         size_t *count, uint32_t srcOffset = 0,
                         uint32_t srcLength = 0) {
	return get_list<uint32_t, Uint32Array>(iso, v,
	                                       !v.IsEmpty() && v->IsUint32Array(),
	                                       tmp, ptr, count, srcOffset,
	                                       srcLength);
}

// Read a JS array of GLenums (e.g. drawBuffers / invalidateFramebuffer).
bool get_enum_list(Isolate *iso, Local<Value> v, std::vector<GLenum> &out) {
	if (v.IsEmpty() || !v->IsArray())
		return false;
	Local<Array> arr = v.As<Array>();
	Local<Context> ctx = cur(iso);
	uint32_t n = arr->Length();
	out.resize(n);
	for (uint32_t i = 0; i < n; i++) {
		Local<Value> el;
		out[i] = 0;
		if (arr->Get(ctx, i).ToLocal(&el))
			out[i] = el->Uint32Value(ctx).FromMaybe(0);
	}
	return true;
}

Local<Value> make_f32_array(Isolate *iso, const float *data, size_t n) {
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, n * 4);
	memcpy(ab->Data(), data, n * 4);
	return Float32Array::New(ab, 0, n);
}
Local<Value> make_i32_array(Isolate *iso, const int32_t *data, size_t n) {
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, n * 4);
	memcpy(ab->Data(), data, n * 4);
	return Int32Array::New(ab, 0, n);
}
Local<Value> make_u32_array(Isolate *iso, const uint32_t *data, size_t n) {
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, n * 4);
	memcpy(ab->Data(), data, n * 4);
	return Uint32Array::New(ab, 0, n);
}

// ---------------------------------------------------------------------------
// Pixel size math (upload/readback validation + flip-Y emulation)
// ---------------------------------------------------------------------------

size_t bytes_per_pixel(GLenum format, GLenum type) {
	switch (type) {
	case GL_UNSIGNED_SHORT_5_6_5:
	case GL_UNSIGNED_SHORT_4_4_4_4:
	case GL_UNSIGNED_SHORT_5_5_5_1:
		return 2;
	case GL_UNSIGNED_INT_2_10_10_10_REV:
	case GL_UNSIGNED_INT_10F_11F_11F_REV:
	case GL_UNSIGNED_INT_5_9_9_9_REV:
	case GL_UNSIGNED_INT_24_8:
		return 4;
	case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
		return 8;
	}
	size_t ts;
	switch (type) {
	case GL_UNSIGNED_BYTE:
	case GL_BYTE:
		ts = 1;
		break;
	case GL_UNSIGNED_SHORT:
	case GL_SHORT:
	case GL_HALF_FLOAT:
		ts = 2;
		break;
	case GL_UNSIGNED_INT:
	case GL_INT:
	case GL_FLOAT:
		ts = 4;
		break;
	default:
		return 0;
	}
	size_t comps;
	switch (format) {
	case GL_RED:
	case GL_RED_INTEGER:
	case GL_ALPHA:
	case GL_LUMINANCE:
	case GL_DEPTH_COMPONENT:
	case GL_STENCIL_INDEX8:
		comps = 1;
		break;
	case GL_RG:
	case GL_RG_INTEGER:
	case GL_LUMINANCE_ALPHA:
	case GL_DEPTH_STENCIL:
		comps = 2;
		break;
	case GL_RGB:
	case GL_RGB_INTEGER:
		comps = 3;
		break;
	case GL_RGBA:
	case GL_RGBA_INTEGER:
		comps = 4;
		break;
	default:
		return 0;
	}
	return ts * comps;
}

size_t row_stride(size_t width, size_t bpp, int alignment, int row_length) {
	size_t w = row_length > 0 ? (size_t)row_length : width;
	size_t row = w * bpp;
	size_t a = alignment > 0 ? (size_t)alignment : 1;
	return (row + a - 1) / a * a;
}

// Required source bytes for a (sub)image upload: full strides for all rows but
// the last, which may be unpadded.
size_t required_upload_bytes(size_t width, size_t height, size_t depth,
                             size_t bpp, int alignment, int row_length) {
	if (width == 0 || height == 0 || depth == 0 || bpp == 0)
		return 0;
	size_t stride = row_stride(width, bpp, alignment, row_length);
	size_t rows = height * depth;
	return stride * (rows - 1) + width * bpp;
}

// Apply UNPACK_FLIP_Y_WEBGL / UNPACK_PREMULTIPLY_ALPHA_WEBGL emulation to a
// pixel upload. Returns the pointer to upload from (either `src` unchanged or
// `tmp.data()` holding the converted copy).
const uint8_t *apply_unpack_emulation(const uint8_t *src, size_t avail,
                                      size_t width, size_t height, size_t bpp,
                                      GLenum format, GLenum type,
                                      std::vector<uint8_t> &tmp) {
	if (!st || (!st->unpack_flip_y && !st->unpack_premultiply))
		return src;
	size_t stride =
	    row_stride(width, bpp, st->unpack_alignment, st->unpack_row_length);
	size_t needed = required_upload_bytes(width, height, 1, bpp,
	                                      st->unpack_alignment,
	                                      st->unpack_row_length);
	if (needed == 0 || avail < needed)
		return src; // caller validates; don't crash here
	tmp.resize(stride * height);
	for (size_t y = 0; y < height; y++) {
		const uint8_t *in = src + y * stride;
		size_t out_y = st->unpack_flip_y ? (height - 1 - y) : y;
		uint8_t *out = tmp.data() + out_y * stride;
		size_t row_bytes = width * bpp;
		if (y == height - 1)
			row_bytes = width * bpp; // last row may be short in src; same size
		memcpy(out, in, row_bytes);
	}
	if (st->unpack_premultiply && format == GL_RGBA &&
	    type == GL_UNSIGNED_BYTE) {
		for (size_t y = 0; y < height; y++) {
			uint8_t *row = tmp.data() + y * stride;
			for (size_t x = 0; x < width; x++) {
				uint8_t *p = row + x * 4;
				uint32_t a = p[3];
				p[0] = (uint8_t)((p[0] * a + 127) / 255);
				p[1] = (uint8_t)((p[1] * a + 127) / 255);
				p[2] = (uint8_t)((p[2] * a + 127) / 255);
			}
		}
	}
	return tmp.data();
}

// ---------------------------------------------------------------------------
// Binding macros
// ---------------------------------------------------------------------------

#define FN(name) static void name(const FunctionCallbackInfo<Value> &info)
#define GUARD()                                                                \
	do {                                                                       \
		if (!st || !st->active)                                                \
			return;                                                            \
	} while (0)

// ---------------------------------------------------------------------------
// Simple state functions
// ---------------------------------------------------------------------------

FN(w_active_texture) { GUARD(); glActiveTexture(a_u32(info, 0)); }
FN(w_blend_color) {
	GUARD();
	glBlendColor(a_f32(info, 0), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3));
}
FN(w_blend_equation) { GUARD(); glBlendEquation(a_u32(info, 0)); }
FN(w_blend_equation_separate) {
	GUARD();
	glBlendEquationSeparate(a_u32(info, 0), a_u32(info, 1));
}
FN(w_blend_func) { GUARD(); glBlendFunc(a_u32(info, 0), a_u32(info, 1)); }
FN(w_blend_func_separate) {
	GUARD();
	glBlendFuncSeparate(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                    a_u32(info, 3));
}
FN(w_clear) {
	GUARD();
	glClear(a_u32(info, 0));
	mark_dirty();
}
FN(w_clear_color) {
	GUARD();
	glClearColor(a_f32(info, 0), a_f32(info, 1), a_f32(info, 2), a_f32(info, 3));
}
FN(w_clear_depth) { GUARD(); glClearDepthf(a_f32(info, 0)); }
FN(w_clear_stencil) { GUARD(); glClearStencil(a_i32(info, 0)); }
FN(w_color_mask) {
	GUARD();
	glColorMask(a_bool(info, 0), a_bool(info, 1), a_bool(info, 2),
	            a_bool(info, 3));
}
FN(w_cull_face) { GUARD(); glCullFace(a_u32(info, 0)); }
FN(w_depth_func) { GUARD(); glDepthFunc(a_u32(info, 0)); }
FN(w_depth_mask) { GUARD(); glDepthMask(a_bool(info, 0)); }
FN(w_depth_range) {
	GUARD();
	glDepthRangef(a_f32(info, 0), a_f32(info, 1));
}
FN(w_disable) { GUARD(); glDisable(a_u32(info, 0)); }
FN(w_enable) { GUARD(); glEnable(a_u32(info, 0)); }
FN(w_finish) { GUARD(); glFinish(); }
FN(w_flush) { GUARD(); glFlush(); }
FN(w_front_face) { GUARD(); glFrontFace(a_u32(info, 0)); }
FN(w_hint) { GUARD(); glHint(a_u32(info, 0), a_u32(info, 1)); }
FN(w_is_enabled) {
	info.GetReturnValue().Set(false);
	GUARD();
	info.GetReturnValue().Set(glIsEnabled(a_u32(info, 0)) == GL_TRUE);
}
FN(w_line_width) { GUARD(); glLineWidth(a_f32(info, 0)); }
FN(w_pixel_storei) {
	GUARD();
	GLenum pname = a_u32(info, 0);
	GLint param = a_i32(info, 1);
	switch (pname) {
	case NX_GL_UNPACK_FLIP_Y_WEBGL:
		st->unpack_flip_y = param != 0;
		return;
	case NX_GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
		st->unpack_premultiply = param != 0;
		return;
	case NX_GL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
		return; // ignored (no colorspace conversion applied)
	case GL_UNPACK_ALIGNMENT:
		st->unpack_alignment = param;
		break;
	case GL_UNPACK_ROW_LENGTH:
		st->unpack_row_length = param;
		break;
	case GL_PACK_ALIGNMENT:
		st->pack_alignment = param;
		break;
	}
	glPixelStorei(pname, param);
}
FN(w_polygon_offset) {
	GUARD();
	glPolygonOffset(a_f32(info, 0), a_f32(info, 1));
}
FN(w_sample_coverage) {
	GUARD();
	glSampleCoverage(a_f32(info, 0), a_bool(info, 1));
}
FN(w_scissor) {
	GUARD();
	glScissor(a_i32(info, 0), a_i32(info, 1), a_i32(info, 2), a_i32(info, 3));
}
FN(w_stencil_func) {
	GUARD();
	glStencilFunc(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2));
}
FN(w_stencil_func_separate) {
	GUARD();
	glStencilFuncSeparate(a_u32(info, 0), a_u32(info, 1), a_i32(info, 2),
	                      a_u32(info, 3));
}
FN(w_stencil_mask) { GUARD(); glStencilMask(a_u32(info, 0)); }
FN(w_stencil_mask_separate) {
	GUARD();
	glStencilMaskSeparate(a_u32(info, 0), a_u32(info, 1));
}
FN(w_stencil_op) {
	GUARD();
	glStencilOp(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2));
}
FN(w_stencil_op_separate) {
	GUARD();
	glStencilOpSeparate(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                    a_u32(info, 3));
}
FN(w_viewport) {
	GUARD();
	glViewport(a_i32(info, 0), a_i32(info, 1), a_i32(info, 2), a_i32(info, 3));
}
FN(w_get_error) {
	info.GetReturnValue().Set((uint32_t)GL_NO_ERROR);
	if (!st || !st->active)
		return;
	if (st->synthetic_error != GL_NO_ERROR) {
		info.GetReturnValue().Set((uint32_t)st->synthetic_error);
		st->synthetic_error = GL_NO_ERROR;
		return;
	}
	info.GetReturnValue().Set((uint32_t)glGetError());
}

// ---------------------------------------------------------------------------
// Object create / delete / is / bind
// ---------------------------------------------------------------------------

#define CREATE_OBJ(NAME, KIND, GENFN)                                          \
	FN(NAME) {                                                                 \
		GUARD();                                                               \
		Isolate *iso = info.GetIsolate();                                      \
		GLuint id = 0;                                                         \
		GENFN(1, &id);                                                         \
		info.GetReturnValue().Set(new_gl_obj(iso, KIND, id));                  \
	}

#define DELETE_OBJ(NAME, DELFN)                                                \
	FN(NAME) {                                                                 \
		GUARD();                                                               \
		GLObj *o = get_gl_obj(info[0]);                                        \
		if (!o || o->id == 0)                                                  \
			return;                                                            \
		GLuint id = o->id;                                                     \
		DELFN(1, &id);                                                         \
		o->id = 0;                                                             \
	}

#define IS_OBJ(NAME, ISFN)                                                     \
	FN(NAME) {                                                                 \
		info.GetReturnValue().Set(false);                                      \
		GUARD();                                                               \
		GLObj *o = get_gl_obj(info[0]);                                        \
		if (!o || o->id == 0)                                                  \
			return;                                                            \
		info.GetReturnValue().Set(ISFN(o->id) == GL_TRUE);                     \
	}

CREATE_OBJ(w_create_buffer, K_BUFFER, glGenBuffers)
CREATE_OBJ(w_create_framebuffer, K_FRAMEBUFFER, glGenFramebuffers)
CREATE_OBJ(w_create_renderbuffer, K_RENDERBUFFER, glGenRenderbuffers)
CREATE_OBJ(w_create_texture, K_TEXTURE, glGenTextures)
CREATE_OBJ(w_create_query, K_QUERY, glGenQueries)
CREATE_OBJ(w_create_sampler, K_SAMPLER, glGenSamplers)
CREATE_OBJ(w_create_transform_feedback, K_TRANSFORM_FEEDBACK,
           glGenTransformFeedbacks)
CREATE_OBJ(w_create_vertex_array, K_VERTEX_ARRAY, glGenVertexArrays)

DELETE_OBJ(w_delete_buffer, glDeleteBuffers)
DELETE_OBJ(w_delete_framebuffer, glDeleteFramebuffers)
DELETE_OBJ(w_delete_renderbuffer, glDeleteRenderbuffers)
DELETE_OBJ(w_delete_texture, glDeleteTextures)
DELETE_OBJ(w_delete_query, glDeleteQueries)
DELETE_OBJ(w_delete_sampler, glDeleteSamplers)
DELETE_OBJ(w_delete_transform_feedback, glDeleteTransformFeedbacks)
DELETE_OBJ(w_delete_vertex_array, glDeleteVertexArrays)

IS_OBJ(w_is_buffer, glIsBuffer)
IS_OBJ(w_is_framebuffer, glIsFramebuffer)
IS_OBJ(w_is_renderbuffer, glIsRenderbuffer)
IS_OBJ(w_is_texture, glIsTexture)
IS_OBJ(w_is_query, glIsQuery)
IS_OBJ(w_is_sampler, glIsSampler)
IS_OBJ(w_is_transform_feedback, glIsTransformFeedback)
IS_OBJ(w_is_vertex_array, glIsVertexArray)
IS_OBJ(w_is_program, glIsProgram)
IS_OBJ(w_is_shader, glIsShader)

FN(w_create_program) {
	GUARD();
	info.GetReturnValue().Set(
	    new_gl_obj(info.GetIsolate(), K_PROGRAM, glCreateProgram()));
}
FN(w_create_shader) {
	GUARD();
	GLuint id = glCreateShader(a_u32(info, 0));
	if (id == 0) {
		info.GetReturnValue().SetNull();
		return;
	}
	info.GetReturnValue().Set(new_gl_obj(info.GetIsolate(), K_SHADER, id));
}
FN(w_delete_program) {
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || o->id == 0)
		return;
	glDeleteProgram(o->id);
	o->id = 0;
}
FN(w_delete_shader) {
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || o->id == 0)
		return;
	glDeleteShader(o->id);
	o->id = 0;
}

FN(w_bind_buffer) { GUARD(); glBindBuffer(a_u32(info, 0), obj_id(info[1])); }
FN(w_bind_framebuffer) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLuint id = obj_id(info[1]);
	if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
		st->draw_fbo = id;
	glBindFramebuffer(target, id);
}
FN(w_bind_renderbuffer) {
	GUARD();
	glBindRenderbuffer(a_u32(info, 0), obj_id(info[1]));
}
FN(w_bind_texture) { GUARD(); glBindTexture(a_u32(info, 0), obj_id(info[1])); }
FN(w_bind_sampler) { GUARD(); glBindSampler(a_u32(info, 0), obj_id(info[1])); }
FN(w_bind_transform_feedback) {
	GUARD();
	glBindTransformFeedback(a_u32(info, 0), obj_id(info[1]));
}
FN(w_bind_vertex_array) { GUARD(); glBindVertexArray(obj_id(info[0])); }
FN(w_bind_buffer_base) {
	GUARD();
	glBindBufferBase(a_u32(info, 0), a_u32(info, 1), obj_id(info[2]));
}
FN(w_bind_buffer_range) {
	GUARD();
	glBindBufferRange(a_u32(info, 0), a_u32(info, 1), obj_id(info[2]),
	                  (GLintptr)a_i64(info, 3), (GLsizeiptr)a_i64(info, 4));
}

// ---------------------------------------------------------------------------
// Buffer data
// ---------------------------------------------------------------------------

FN(w_buffer_data) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLenum target = a_u32(info, 0);
	GLenum usage = a_u32(info, 2);
	Local<Value> src = info[1];
	if (src->IsNumber()) {
		glBufferData(target, (GLsizeiptr)a_i64(info, 1), nullptr, usage);
		return;
	}
	if (src->IsNullOrUndefined()) {
		glBufferData(target, 0, nullptr, usage);
		return;
	}
	size_t len = 0, elem = 1;
	uint8_t *bytes = nullptr;
	if (src->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = src.As<ArrayBuffer>();
		bytes = (uint8_t *)ab->Data();
		len = ab->ByteLength();
	} else {
		bytes = view_bytes(src, &len, &elem);
	}
	if (!bytes && len > 0) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	// WebGL2 overload: bufferData(target, srcData, usage, srcOffset[, length])
	if (info.Length() > 3) {
		uint64_t srcOffset = (uint64_t)a_i64(info, 3) * elem;
		uint64_t srcLen =
		    info.Length() > 4 ? (uint64_t)a_i64(info, 4) * elem : 0;
		if (srcOffset > len || (srcLen && srcOffset + srcLen > len)) {
			record_error(GL_INVALID_VALUE);
			return;
		}
		bytes += srcOffset;
		len = srcLen ? srcLen : len - srcOffset;
	}
	(void)iso;
	glBufferData(target, (GLsizeiptr)len, bytes, usage);
}

FN(w_buffer_sub_data) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLintptr dstOffset = (GLintptr)a_i64(info, 1);
	Local<Value> src = info[2];
	size_t len = 0, elem = 1;
	uint8_t *bytes = nullptr;
	if (src->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = src.As<ArrayBuffer>();
		bytes = (uint8_t *)ab->Data();
		len = ab->ByteLength();
	} else {
		bytes = view_bytes(src, &len, &elem);
	}
	if (!bytes) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	if (info.Length() > 3) {
		uint64_t srcOffset = (uint64_t)a_i64(info, 3) * elem;
		uint64_t srcLen =
		    info.Length() > 4 ? (uint64_t)a_i64(info, 4) * elem : 0;
		if (srcOffset > len || (srcLen && srcOffset + srcLen > len)) {
			record_error(GL_INVALID_VALUE);
			return;
		}
		bytes += srcOffset;
		len = srcLen ? srcLen : len - srcOffset;
	}
	glBufferSubData(target, dstOffset, (GLsizeiptr)len, bytes);
}

FN(w_copy_buffer_sub_data) {
	GUARD();
	glCopyBufferSubData(a_u32(info, 0), a_u32(info, 1),
	                    (GLintptr)a_i64(info, 2), (GLintptr)a_i64(info, 3),
	                    (GLsizeiptr)a_i64(info, 4));
}

// getBufferSubData: ES3 has no glGetBufferSubData; emulate with
// glMapBufferRange(GL_MAP_READ_BIT) + memcpy.
FN(w_get_buffer_sub_data) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLintptr srcOffset = (GLintptr)a_i64(info, 1);
	size_t len = 0, elem = 1;
	uint8_t *dst = view_bytes(info[2], &len, &elem);
	if (!dst) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	uint64_t dstOffset =
	    info.Length() > 3 ? (uint64_t)a_i64(info, 3) * elem : 0;
	uint64_t copyLen = info.Length() > 4 ? (uint64_t)a_i64(info, 4) * elem : 0;
	if (dstOffset > len || (copyLen && dstOffset + copyLen > len)) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	if (copyLen == 0)
		copyLen = len - dstOffset;
	if (copyLen == 0)
		return;
	void *mapped =
	    glMapBufferRange(target, srcOffset, (GLsizeiptr)copyLen, GL_MAP_READ_BIT);
	if (!mapped) {
		record_error(GL_INVALID_OPERATION);
		return;
	}
	memcpy(dst + dstOffset, mapped, copyLen);
	glUnmapBuffer(target);
}

FN(w_get_buffer_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLenum target = a_u32(info, 0);
	GLenum pname = a_u32(info, 1);
	if (pname == GL_BUFFER_SIZE) {
		GLint64 v = 0;
		glGetBufferParameteri64v(target, pname, &v);
		info.GetReturnValue().Set((double)v);
	} else {
		GLint v = 0;
		glGetBufferParameteriv(target, pname, &v);
		info.GetReturnValue().Set(v);
	}
}

// ---------------------------------------------------------------------------
// Shaders & programs
// ---------------------------------------------------------------------------

FN(w_attach_shader) {
	GUARD();
	glAttachShader(obj_id(info[0]), obj_id(info[1]));
}
FN(w_detach_shader) {
	GUARD();
	glDetachShader(obj_id(info[0]), obj_id(info[1]));
}
FN(w_compile_shader) { GUARD(); glCompileShader(obj_id(info[0])); }
FN(w_link_program) { GUARD(); glLinkProgram(obj_id(info[0])); }
FN(w_use_program) { GUARD(); glUseProgram(obj_id(info[0])); }
FN(w_validate_program) { GUARD(); glValidateProgram(obj_id(info[0])); }
FN(w_shader_source) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint shader = obj_id(info[0]);
	String::Utf8Value src(iso, info[1]);
	if (!*src)
		return;
	const char *s = *src;
	GLint len = (GLint)src.length();
	glShaderSource(shader, 1, &s, &len);
}
FN(w_get_shader_source) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint shader = obj_id(info[0]);
	GLint len = 0;
	glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &len);
	std::vector<char> buf((size_t)len + 1);
	GLsizei out = 0;
	glGetShaderSource(shader, (GLsizei)buf.size(), &out, buf.data());
	info.GetReturnValue().Set(nx_str_lossy(iso, buf.data(), out));
}
FN(w_get_shader_info_log) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint shader = obj_id(info[0]);
	GLint len = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
	std::vector<char> buf((size_t)len + 1);
	GLsizei out = 0;
	glGetShaderInfoLog(shader, (GLsizei)buf.size(), &out, buf.data());
	info.GetReturnValue().Set(nx_str_lossy(iso, buf.data(), out));
}
FN(w_get_program_info_log) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint program = obj_id(info[0]);
	GLint len = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
	std::vector<char> buf((size_t)len + 1);
	GLsizei out = 0;
	glGetProgramInfoLog(program, (GLsizei)buf.size(), &out, buf.data());
	info.GetReturnValue().Set(nx_str_lossy(iso, buf.data(), out));
}
FN(w_get_shader_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLuint shader = obj_id(info[0]);
	GLenum pname = a_u32(info, 1);
	GLint v = 0;
	glGetShaderiv(shader, pname, &v);
	switch (pname) {
	case GL_DELETE_STATUS:
	case GL_COMPILE_STATUS:
		info.GetReturnValue().Set(v == GL_TRUE);
		break;
	default:
		info.GetReturnValue().Set(v);
		break;
	}
}
FN(w_get_program_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLuint program = obj_id(info[0]);
	GLenum pname = a_u32(info, 1);
	GLint v = 0;
	glGetProgramiv(program, pname, &v);
	switch (pname) {
	case GL_DELETE_STATUS:
	case GL_LINK_STATUS:
	case GL_VALIDATE_STATUS:
		info.GetReturnValue().Set(v == GL_TRUE);
		break;
	default:
		info.GetReturnValue().Set(v);
		break;
	}
}
FN(w_get_shader_precision_format) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	GLint range[2] = {0, 0};
	GLint precision = 0;
	glGetShaderPrecisionFormat(a_u32(info, 0), a_u32(info, 1), range,
	                           &precision);
	Local<Object> o = Object::New(iso);
	if (!st->protos[K_SHADER_PRECISION_FORMAT].IsEmpty()) {
		o->SetPrototype(ctx, st->protos[K_SHADER_PRECISION_FORMAT].Get(iso))
		    .Check();
	}
	o->Set(ctx, nx_str(iso, "rangeMin"), Integer::New(iso, range[0])).Check();
	o->Set(ctx, nx_str(iso, "rangeMax"), Integer::New(iso, range[1])).Check();
	o->Set(ctx, nx_str(iso, "precision"), Integer::New(iso, precision)).Check();
	info.GetReturnValue().Set(o);
}
FN(w_get_attached_shaders) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	GLuint program = obj_id(info[0]);
	GLuint shaders[16];
	GLsizei count = 0;
	glGetAttachedShaders(program, 16, &count, shaders);
	Local<Array> arr = Array::New(iso, count);
	for (GLsizei i = 0; i < count; i++) {
		arr->Set(ctx, i, new_gl_obj(iso, K_SHADER, shaders[i])).Check();
	}
	info.GetReturnValue().Set(arr);
}
FN(w_bind_attrib_location) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[2]);
	if (!*name)
		return;
	glBindAttribLocation(obj_id(info[0]), a_u32(info, 1), *name);
}
FN(w_get_attrib_location) {
	info.GetReturnValue().Set(-1);
	GUARD();
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[1]);
	if (!*name)
		return;
	info.GetReturnValue().Set(glGetAttribLocation(obj_id(info[0]), *name));
}
FN(w_get_frag_data_location) {
	info.GetReturnValue().Set(-1);
	GUARD();
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[1]);
	if (!*name)
		return;
	info.GetReturnValue().Set(glGetFragDataLocation(obj_id(info[0]), *name));
}
FN(w_get_uniform_location) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[1]);
	if (!*name)
		return;
	GLint loc = glGetUniformLocation(obj_id(info[0]), *name);
	if (loc < 0)
		return;
	info.GetReturnValue().Set(new_gl_obj(iso, K_UNIFORM_LOCATION, 0, nullptr,
	                                     loc));
}

static void active_info_common(const FunctionCallbackInfo<Value> &info,
                               bool attrib) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	GLuint program = obj_id(info[0]);
	GLuint index = a_u32(info, 1);
	GLint count = 0;
	glGetProgramiv(program, attrib ? GL_ACTIVE_ATTRIBUTES : GL_ACTIVE_UNIFORMS,
	               &count);
	if ((GLint)index >= count) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	char name[256];
	GLsizei len = 0;
	GLint size = 0;
	GLenum type = 0;
	if (attrib)
		glGetActiveAttrib(program, index, sizeof(name), &len, &size, &type,
		                  name);
	else
		glGetActiveUniform(program, index, sizeof(name), &len, &size, &type,
		                   name);
	Local<Object> o = Object::New(iso);
	if (!st->protos[K_ACTIVE_INFO].IsEmpty()) {
		o->SetPrototype(ctx, st->protos[K_ACTIVE_INFO].Get(iso)).Check();
	}
	o->Set(ctx, nx_str(iso, "name"), nx_str_lossy(iso, name, len)).Check();
	o->Set(ctx, nx_str(iso, "size"), Integer::New(iso, size)).Check();
	o->Set(ctx, nx_str(iso, "type"), Integer::NewFromUnsigned(iso, type))
	    .Check();
	info.GetReturnValue().Set(o);
}
FN(w_get_active_attrib) { active_info_common(info, true); }
FN(w_get_active_uniform) { active_info_common(info, false); }

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

FN(w_tex_parameterf) {
	GUARD();
	glTexParameterf(a_u32(info, 0), a_u32(info, 1), a_f32(info, 2));
}
FN(w_tex_parameteri) {
	GUARD();
	glTexParameteri(a_u32(info, 0), a_u32(info, 1), a_i32(info, 2));
}
FN(w_get_tex_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLenum target = a_u32(info, 0);
	GLenum pname = a_u32(info, 1);
	switch (pname) {
	case GL_TEXTURE_MAX_LOD:
	case GL_TEXTURE_MIN_LOD: {
		GLfloat f = 0;
		glGetTexParameterfv(target, pname, &f);
		info.GetReturnValue().Set((double)f);
		return;
	}
	case GL_TEXTURE_IMMUTABLE_FORMAT: {
		GLint v = 0;
		glGetTexParameteriv(target, pname, &v);
		info.GetReturnValue().Set(v == GL_TRUE);
		return;
	}
	}
	GLint v = 0;
	glGetTexParameteriv(target, pname, &v);
	info.GetReturnValue().Set(v);
}
FN(w_generate_mipmap) { GUARD(); glGenerateMipmap(a_u32(info, 0)); }

// texImage2D / texSubImage2D with an ArrayBufferView | null | PBO-offset
// source. The TS wrapper normalizes TexImageSource objects (ImageData, Image,
// ImageBitmap, canvases) into the ArrayBufferView form before calling this.
static void tex_image_2d_common(const FunctionCallbackInfo<Value> &info,
                                bool sub) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLint p2 = a_i32(info, 2); // internalformat (tex) / xoffset (sub)
	GLint p3 = a_i32(info, 3); // yoffset (sub only)
	GLsizei w, h;
	GLint border = 0;
	if (sub) {
		w = a_i32(info, 4);
		h = a_i32(info, 5);
	} else {
		w = a_i32(info, 3);
		h = a_i32(info, 4);
		border = a_i32(info, 5);
	}
	GLenum format = a_u32(info, 6);
	GLenum type = a_u32(info, 7);
	Local<Value> src = info[8];
	const void *ptr = nullptr;
	std::vector<uint8_t> tmp;
	if (src->IsNumber()) {
		// PBO offset variant.
		ptr = (const void *)(uintptr_t)a_i64(info, 8);
	} else if (!src->IsNullOrUndefined()) {
		size_t len = 0, elem = 1;
		uint8_t *bytes = view_bytes(src, &len, &elem);
		if (!bytes) {
			record_error(GL_INVALID_VALUE);
			return;
		}
		uint64_t srcOffset =
		    info.Length() > 9 ? (uint64_t)a_i64(info, 9) * elem : 0;
		if (srcOffset > len) {
			record_error(GL_INVALID_VALUE);
			return;
		}
		size_t bpp = bytes_per_pixel(format, type);
		size_t need = required_upload_bytes(w, h, 1, bpp, st->unpack_alignment,
		                                    st->unpack_row_length);
		if (bpp == 0 || len - srcOffset < need) {
			record_error(GL_INVALID_OPERATION);
			return;
		}
		ptr = apply_unpack_emulation(bytes + srcOffset, len - srcOffset, w, h,
		                             bpp, format, type, tmp);
	}
	if (sub)
		glTexSubImage2D(target, level, p2, p3, w, h, format, type, ptr);
	else
		glTexImage2D(target, level, p2, w, h, border, format, type, ptr);
}
FN(w_tex_image_2d) { tex_image_2d_common(info, false); }
FN(w_tex_sub_image_2d) { tex_image_2d_common(info, true); }

// texImage3D(target, level, internalformat, w, h, depth, border, format, type,
//            src[, srcOffset])
FN(w_tex_image_3d) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLint internalformat = a_i32(info, 2);
	GLsizei w = a_i32(info, 3), h = a_i32(info, 4), d = a_i32(info, 5);
	GLint border = a_i32(info, 6);
	GLenum format = a_u32(info, 7);
	GLenum type = a_u32(info, 8);
	Local<Value> src = info[9];
	const void *ptr = nullptr;
	if (src->IsNumber()) {
		ptr = (const void *)(uintptr_t)a_i64(info, 9);
	} else if (!src->IsNullOrUndefined()) {
		size_t len = 0, elem = 1;
		uint8_t *bytes = view_bytes(src, &len, &elem);
		if (!bytes) {
			record_error(GL_INVALID_VALUE);
			return;
		}
		uint64_t srcOffset =
		    info.Length() > 10 ? (uint64_t)a_i64(info, 10) * elem : 0;
		size_t bpp = bytes_per_pixel(format, type);
		size_t need = required_upload_bytes(w, h, d, bpp, st->unpack_alignment,
		                                    st->unpack_row_length);
		if (srcOffset > len || bpp == 0 || len - srcOffset < need) {
			record_error(GL_INVALID_OPERATION);
			return;
		}
		ptr = bytes + srcOffset;
	}
	glTexImage3D(target, level, internalformat, w, h, d, border, format, type,
	             ptr);
}

// texSubImage3D(target, level, xo, yo, zo, w, h, depth, format, type,
//               src[, srcOffset])
FN(w_tex_sub_image_3d) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLint xo = a_i32(info, 2), yo = a_i32(info, 3), zo = a_i32(info, 4);
	GLsizei w = a_i32(info, 5), h = a_i32(info, 6), d = a_i32(info, 7);
	GLenum format = a_u32(info, 8);
	GLenum type = a_u32(info, 9);
	Local<Value> src = info[10];
	const void *ptr = nullptr;
	if (src->IsNumber()) {
		ptr = (const void *)(uintptr_t)a_i64(info, 10);
	} else if (!src->IsNullOrUndefined()) {
		size_t len = 0, elem = 1;
		uint8_t *bytes = view_bytes(src, &len, &elem);
		if (!bytes) {
			record_error(GL_INVALID_VALUE);
			return;
		}
		uint64_t srcOffset =
		    info.Length() > 11 ? (uint64_t)a_i64(info, 11) * elem : 0;
		size_t bpp = bytes_per_pixel(format, type);
		size_t need = required_upload_bytes(w, h, d, bpp, st->unpack_alignment,
		                                    st->unpack_row_length);
		if (srcOffset > len || bpp == 0 || len - srcOffset < need) {
			record_error(GL_INVALID_OPERATION);
			return;
		}
		ptr = bytes + srcOffset;
	}
	glTexSubImage3D(target, level, xo, yo, zo, w, h, d, format, type, ptr);
}

FN(w_tex_storage_2d) {
	GUARD();
	glTexStorage2D(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	               a_i32(info, 3), a_i32(info, 4));
}
FN(w_tex_storage_3d) {
	GUARD();
	glTexStorage3D(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	               a_i32(info, 3), a_i32(info, 4), a_i32(info, 5));
}

FN(w_copy_tex_image_2d) {
	GUARD();
	glCopyTexImage2D(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	                 a_i32(info, 3), a_i32(info, 4), a_i32(info, 5),
	                 a_i32(info, 6), a_i32(info, 7));
}
FN(w_copy_tex_sub_image_2d) {
	GUARD();
	glCopyTexSubImage2D(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2),
	                    a_i32(info, 3), a_i32(info, 4), a_i32(info, 5),
	                    a_i32(info, 6), a_i32(info, 7));
}
FN(w_copy_tex_sub_image_3d) {
	GUARD();
	glCopyTexSubImage3D(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2),
	                    a_i32(info, 3), a_i32(info, 4), a_i32(info, 5),
	                    a_i32(info, 6), a_i32(info, 7), a_i32(info, 8));
}

// compressedTexImage2D(target, level, internalformat, w, h, border,
//   srcData[, srcOffset, srcLengthOverride]) or (..., imageSize, offset)
FN(w_compressed_tex_image_2d) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLenum internalformat = a_u32(info, 2);
	GLsizei w = a_i32(info, 3), h = a_i32(info, 4);
	GLint border = a_i32(info, 5);
	if (info[6]->IsNumber()) {
		glCompressedTexImage2D(target, level, internalformat, w, h, border,
		                       a_i32(info, 6),
		                       (const void *)(uintptr_t)a_i64(info, 7));
		return;
	}
	size_t len = 0, elem = 1;
	uint8_t *bytes = view_bytes(info[6], &len, &elem);
	if (!bytes) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	uint64_t srcOffset = info.Length() > 7 ? (uint64_t)a_i64(info, 7) * elem : 0;
	uint64_t srcLen = info.Length() > 8 ? (uint64_t)a_i64(info, 8) * elem : 0;
	if (srcOffset > len || (srcLen && srcOffset + srcLen > len)) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	if (srcLen == 0)
		srcLen = len - srcOffset;
	glCompressedTexImage2D(target, level, internalformat, w, h, border,
	                       (GLsizei)srcLen, bytes + srcOffset);
}
FN(w_compressed_tex_sub_image_2d) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLint xo = a_i32(info, 2), yo = a_i32(info, 3);
	GLsizei w = a_i32(info, 4), h = a_i32(info, 5);
	GLenum format = a_u32(info, 6);
	if (info[7]->IsNumber()) {
		glCompressedTexSubImage2D(target, level, xo, yo, w, h, format,
		                          a_i32(info, 7),
		                          (const void *)(uintptr_t)a_i64(info, 8));
		return;
	}
	size_t len = 0, elem = 1;
	uint8_t *bytes = view_bytes(info[7], &len, &elem);
	if (!bytes) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	uint64_t srcOffset = info.Length() > 8 ? (uint64_t)a_i64(info, 8) * elem : 0;
	uint64_t srcLen = info.Length() > 9 ? (uint64_t)a_i64(info, 9) * elem : 0;
	if (srcOffset > len || (srcLen && srcOffset + srcLen > len)) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	if (srcLen == 0)
		srcLen = len - srcOffset;
	glCompressedTexSubImage2D(target, level, xo, yo, w, h, format,
	                          (GLsizei)srcLen, bytes + srcOffset);
}
FN(w_compressed_tex_image_3d) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLenum internalformat = a_u32(info, 2);
	GLsizei w = a_i32(info, 3), h = a_i32(info, 4), d = a_i32(info, 5);
	GLint border = a_i32(info, 6);
	if (info[7]->IsNumber()) {
		glCompressedTexImage3D(target, level, internalformat, w, h, d, border,
		                       a_i32(info, 7),
		                       (const void *)(uintptr_t)a_i64(info, 8));
		return;
	}
	size_t len = 0, elem = 1;
	uint8_t *bytes = view_bytes(info[7], &len, &elem);
	if (!bytes) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	uint64_t srcOffset = info.Length() > 8 ? (uint64_t)a_i64(info, 8) * elem : 0;
	uint64_t srcLen = info.Length() > 9 ? (uint64_t)a_i64(info, 9) * elem : 0;
	if (srcOffset > len || (srcLen && srcOffset + srcLen > len)) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	if (srcLen == 0)
		srcLen = len - srcOffset;
	glCompressedTexImage3D(target, level, internalformat, w, h, d, border,
	                       (GLsizei)srcLen, bytes + srcOffset);
}
FN(w_compressed_tex_sub_image_3d) {
	GUARD();
	GLenum target = a_u32(info, 0);
	GLint level = a_i32(info, 1);
	GLint xo = a_i32(info, 2), yo = a_i32(info, 3), zo = a_i32(info, 4);
	GLsizei w = a_i32(info, 5), h = a_i32(info, 6), d = a_i32(info, 7);
	GLenum format = a_u32(info, 8);
	if (info[9]->IsNumber()) {
		glCompressedTexSubImage3D(target, level, xo, yo, zo, w, h, d, format,
		                          a_i32(info, 9),
		                          (const void *)(uintptr_t)a_i64(info, 10));
		return;
	}
	size_t len = 0, elem = 1;
	uint8_t *bytes = view_bytes(info[9], &len, &elem);
	if (!bytes) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	uint64_t srcOffset =
	    info.Length() > 10 ? (uint64_t)a_i64(info, 10) * elem : 0;
	uint64_t srcLen = info.Length() > 11 ? (uint64_t)a_i64(info, 11) * elem : 0;
	if (srcOffset > len || (srcLen && srcOffset + srcLen > len)) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	if (srcLen == 0)
		srcLen = len - srcOffset;
	glCompressedTexSubImage3D(target, level, xo, yo, zo, w, h, d, format,
	                          (GLsizei)srcLen, bytes + srcOffset);
}

// ---------------------------------------------------------------------------
// Framebuffers & renderbuffers
// ---------------------------------------------------------------------------

FN(w_check_framebuffer_status) {
	info.GetReturnValue().Set(0);
	GUARD();
	info.GetReturnValue().Set(
	    (uint32_t)glCheckFramebufferStatus(a_u32(info, 0)));
}
FN(w_framebuffer_renderbuffer) {
	GUARD();
	glFramebufferRenderbuffer(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                          obj_id(info[3]));
}
FN(w_framebuffer_texture_2d) {
	GUARD();
	glFramebufferTexture2D(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                       obj_id(info[3]), a_i32(info, 4));
}
FN(w_framebuffer_texture_layer) {
	GUARD();
	glFramebufferTextureLayer(a_u32(info, 0), a_u32(info, 1), obj_id(info[2]),
	                          a_i32(info, 3), a_i32(info, 4));
}
FN(w_get_framebuffer_attachment_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLenum pname = a_u32(info, 2);
	if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
		return; // object identity not tracked; null
	GLint v = 0;
	glGetFramebufferAttachmentParameteriv(a_u32(info, 0), a_u32(info, 1),
	                                      pname, &v);
	info.GetReturnValue().Set(v);
}
FN(w_get_renderbuffer_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLint v = 0;
	glGetRenderbufferParameteriv(a_u32(info, 0), a_u32(info, 1), &v);
	info.GetReturnValue().Set(v);
}
FN(w_renderbuffer_storage) {
	GUARD();
	glRenderbufferStorage(a_u32(info, 0), a_u32(info, 1), a_i32(info, 2),
	                      a_i32(info, 3));
}
FN(w_renderbuffer_storage_multisample) {
	GUARD();
	glRenderbufferStorageMultisample(a_u32(info, 0), a_i32(info, 1),
	                                 a_u32(info, 2), a_i32(info, 3),
	                                 a_i32(info, 4));
}
FN(w_blit_framebuffer) {
	GUARD();
	glBlitFramebuffer(a_i32(info, 0), a_i32(info, 1), a_i32(info, 2),
	                  a_i32(info, 3), a_i32(info, 4), a_i32(info, 5),
	                  a_i32(info, 6), a_i32(info, 7), a_u32(info, 8),
	                  a_u32(info, 9));
	mark_dirty();
}
FN(w_invalidate_framebuffer) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	std::vector<GLenum> attachments;
	if (!get_enum_list(iso, info[1], attachments))
		return;
	glInvalidateFramebuffer(a_u32(info, 0), (GLsizei)attachments.size(),
	                        attachments.data());
}
FN(w_invalidate_sub_framebuffer) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	std::vector<GLenum> attachments;
	if (!get_enum_list(iso, info[1], attachments))
		return;
	glInvalidateSubFramebuffer(a_u32(info, 0), (GLsizei)attachments.size(),
	                           attachments.data(), a_i32(info, 2),
	                           a_i32(info, 3), a_i32(info, 4), a_i32(info, 5));
}
FN(w_read_buffer) { GUARD(); glReadBuffer(a_u32(info, 0)); }
FN(w_draw_buffers) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	std::vector<GLenum> bufs;
	if (!get_enum_list(iso, info[0], bufs))
		return;
	glDrawBuffers((GLsizei)bufs.size(), bufs.data());
}
FN(w_get_internalformat_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLenum target = a_u32(info, 0);
	GLenum internalformat = a_u32(info, 1);
	GLenum pname = a_u32(info, 2);
	if (pname != GL_SAMPLES)
		return;
	GLint count = 0;
	glGetInternalformativ(target, internalformat, GL_NUM_SAMPLE_COUNTS, 1,
	                      &count);
	std::vector<GLint> samples((size_t)count);
	if (count > 0)
		glGetInternalformativ(target, internalformat, GL_SAMPLES, count,
		                      samples.data());
	info.GetReturnValue().Set(make_i32_array(iso, samples.data(), samples.size()));
}

FN(w_read_pixels) {
	GUARD();
	GLint x = a_i32(info, 0), y = a_i32(info, 1);
	GLsizei w = a_i32(info, 2), h = a_i32(info, 3);
	GLenum format = a_u32(info, 4), type = a_u32(info, 5);
	if (info[6]->IsNumber()) {
		// PBO offset variant.
		glReadPixels(x, y, w, h, format, type,
		             (void *)(uintptr_t)a_i64(info, 6));
		return;
	}
	size_t len = 0, elem = 1;
	uint8_t *dst = view_bytes(info[6], &len, &elem);
	if (!dst) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	uint64_t dstOffset = info.Length() > 7 ? (uint64_t)a_i64(info, 7) * elem : 0;
	if (dstOffset > len) {
		record_error(GL_INVALID_VALUE);
		return;
	}
	size_t bpp = bytes_per_pixel(format, type);
	size_t need = required_upload_bytes(w, h, 1, bpp, st->pack_alignment, 0);
	if (bpp == 0 || len - dstOffset < need) {
		record_error(GL_INVALID_OPERATION);
		return;
	}
	glReadPixels(x, y, w, h, format, type, dst + dstOffset);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

FN(w_draw_arrays) {
	GUARD();
	glDrawArrays(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2));
	mark_dirty();
}
FN(w_draw_elements) {
	GUARD();
	glDrawElements(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	               (const void *)(uintptr_t)a_i64(info, 3));
	mark_dirty();
}
FN(w_draw_arrays_instanced) {
	GUARD();
	glDrawArraysInstanced(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2),
	                      a_i32(info, 3));
	mark_dirty();
}
FN(w_draw_elements_instanced) {
	GUARD();
	glDrawElementsInstanced(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	                        (const void *)(uintptr_t)a_i64(info, 3),
	                        a_i32(info, 4));
	mark_dirty();
}
FN(w_draw_range_elements) {
	GUARD();
	glDrawRangeElements(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                    a_i32(info, 3), a_u32(info, 4),
	                    (const void *)(uintptr_t)a_i64(info, 5));
	mark_dirty();
}

FN(w_clear_bufferfv) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	std::vector<float> tmp;
	const float *d;
	size_t n;
	uint32_t so = info.Length() > 3 ? a_u32(info, 3) : 0;
	if (!get_f32_list(iso, info[2], tmp, &d, &n, so, 0))
		return;
	if (n < 1)
		return;
	glClearBufferfv(a_u32(info, 0), a_i32(info, 1), d);
	mark_dirty();
}
FN(w_clear_bufferiv) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	std::vector<int32_t> tmp;
	const int32_t *d;
	size_t n;
	uint32_t so = info.Length() > 3 ? a_u32(info, 3) : 0;
	if (!get_i32_list(iso, info[2], tmp, &d, &n, so, 0))
		return;
	if (n < 1)
		return;
	glClearBufferiv(a_u32(info, 0), a_i32(info, 1), d);
	mark_dirty();
}
FN(w_clear_bufferuiv) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	std::vector<uint32_t> tmp;
	const uint32_t *d;
	size_t n;
	uint32_t so = info.Length() > 3 ? a_u32(info, 3) : 0;
	if (!get_u32_list(iso, info[2], tmp, &d, &n, so, 0))
		return;
	if (n < 1)
		return;
	glClearBufferuiv(a_u32(info, 0), a_i32(info, 1), d);
	mark_dirty();
}
FN(w_clear_bufferfi) {
	GUARD();
	glClearBufferfi(a_u32(info, 0), a_i32(info, 1), a_f32(info, 2),
	                a_i32(info, 3));
	mark_dirty();
}

// ---------------------------------------------------------------------------
// Uniforms
// ---------------------------------------------------------------------------

// Per WebGL spec, a null uniform location is silently ignored.
#define UNIFORM_PREAMBLE                                                       \
	GUARD();                                                                   \
	GLObj *lo = get_gl_obj(info[0]);                                           \
	if (!lo)                                                                   \
		return;                                                                \
	GLint loc = lo->loc;                                                       \
	(void)loc;

FN(w_uniform1f) { UNIFORM_PREAMBLE glUniform1f(loc, a_f32(info, 1)); }
FN(w_uniform2f) {
	UNIFORM_PREAMBLE glUniform2f(loc, a_f32(info, 1), a_f32(info, 2));
}
FN(w_uniform3f) {
	UNIFORM_PREAMBLE glUniform3f(loc, a_f32(info, 1), a_f32(info, 2),
	                             a_f32(info, 3));
}
FN(w_uniform4f) {
	UNIFORM_PREAMBLE glUniform4f(loc, a_f32(info, 1), a_f32(info, 2),
	                             a_f32(info, 3), a_f32(info, 4));
}
FN(w_uniform1i) { UNIFORM_PREAMBLE glUniform1i(loc, a_i32(info, 1)); }
FN(w_uniform2i) {
	UNIFORM_PREAMBLE glUniform2i(loc, a_i32(info, 1), a_i32(info, 2));
}
FN(w_uniform3i) {
	UNIFORM_PREAMBLE glUniform3i(loc, a_i32(info, 1), a_i32(info, 2),
	                             a_i32(info, 3));
}
FN(w_uniform4i) {
	UNIFORM_PREAMBLE glUniform4i(loc, a_i32(info, 1), a_i32(info, 2),
	                             a_i32(info, 3), a_i32(info, 4));
}
FN(w_uniform1ui) { UNIFORM_PREAMBLE glUniform1ui(loc, a_u32(info, 1)); }
FN(w_uniform2ui) {
	UNIFORM_PREAMBLE glUniform2ui(loc, a_u32(info, 1), a_u32(info, 2));
}
FN(w_uniform3ui) {
	UNIFORM_PREAMBLE glUniform3ui(loc, a_u32(info, 1), a_u32(info, 2),
	                              a_u32(info, 3));
}
FN(w_uniform4ui) {
	UNIFORM_PREAMBLE glUniform4ui(loc, a_u32(info, 1), a_u32(info, 2),
	                              a_u32(info, 3), a_u32(info, 4));
}

#define UNIFORM_VEC(NAME, N, TYPE, GETLIST, GLCALL)                            \
	FN(NAME) {                                                                 \
		UNIFORM_PREAMBLE;                                                      \
		Isolate *iso = info.GetIsolate();                                      \
		std::vector<TYPE> tmp;                                                 \
		const TYPE *d;                                                         \
		size_t n;                                                              \
		uint32_t so = info.Length() > 2 ? a_u32(info, 2) : 0;                  \
		uint32_t sl = info.Length() > 3 ? a_u32(info, 3) : 0;                  \
		if (!GETLIST(iso, info[1], tmp, &d, &n, so, sl))                       \
			return;                                                            \
		if (n < N) {                                                           \
			record_error(GL_INVALID_VALUE);                                    \
			return;                                                            \
		}                                                                      \
		GLCALL(loc, (GLsizei)(n / N), d);                                      \
	}

UNIFORM_VEC(w_uniform1fv, 1, float, get_f32_list, glUniform1fv)
UNIFORM_VEC(w_uniform2fv, 2, float, get_f32_list, glUniform2fv)
UNIFORM_VEC(w_uniform3fv, 3, float, get_f32_list, glUniform3fv)
UNIFORM_VEC(w_uniform4fv, 4, float, get_f32_list, glUniform4fv)
UNIFORM_VEC(w_uniform1iv, 1, int32_t, get_i32_list, glUniform1iv)
UNIFORM_VEC(w_uniform2iv, 2, int32_t, get_i32_list, glUniform2iv)
UNIFORM_VEC(w_uniform3iv, 3, int32_t, get_i32_list, glUniform3iv)
UNIFORM_VEC(w_uniform4iv, 4, int32_t, get_i32_list, glUniform4iv)
UNIFORM_VEC(w_uniform1uiv, 1, uint32_t, get_u32_list, glUniform1uiv)
UNIFORM_VEC(w_uniform2uiv, 2, uint32_t, get_u32_list, glUniform2uiv)
UNIFORM_VEC(w_uniform3uiv, 3, uint32_t, get_u32_list, glUniform3uiv)
UNIFORM_VEC(w_uniform4uiv, 4, uint32_t, get_u32_list, glUniform4uiv)

// uniformMatrixNfv(location, transpose, data[, srcOffset, srcLength])
#define UNIFORM_MATRIX(NAME, COMP, GLCALL)                                     \
	FN(NAME) {                                                                 \
		UNIFORM_PREAMBLE;                                                      \
		Isolate *iso = info.GetIsolate();                                      \
		bool transpose = a_bool(info, 1);                                      \
		std::vector<float> tmp;                                                \
		const float *d;                                                        \
		size_t n;                                                              \
		uint32_t so = info.Length() > 3 ? a_u32(info, 3) : 0;                  \
		uint32_t sl = info.Length() > 4 ? a_u32(info, 4) : 0;                  \
		if (!get_f32_list(iso, info[2], tmp, &d, &n, so, sl))                  \
			return;                                                            \
		if (n < COMP) {                                                        \
			record_error(GL_INVALID_VALUE);                                    \
			return;                                                            \
		}                                                                      \
		GLCALL(loc, (GLsizei)(n / COMP), transpose, d);                        \
	}

UNIFORM_MATRIX(w_uniform_matrix2fv, 4, glUniformMatrix2fv)
UNIFORM_MATRIX(w_uniform_matrix3fv, 9, glUniformMatrix3fv)
UNIFORM_MATRIX(w_uniform_matrix4fv, 16, glUniformMatrix4fv)
UNIFORM_MATRIX(w_uniform_matrix2x3fv, 6, glUniformMatrix2x3fv)
UNIFORM_MATRIX(w_uniform_matrix3x2fv, 6, glUniformMatrix3x2fv)
UNIFORM_MATRIX(w_uniform_matrix2x4fv, 8, glUniformMatrix2x4fv)
UNIFORM_MATRIX(w_uniform_matrix4x2fv, 8, glUniformMatrix4x2fv)
UNIFORM_MATRIX(w_uniform_matrix3x4fv, 12, glUniformMatrix3x4fv)
UNIFORM_MATRIX(w_uniform_matrix4x3fv, 12, glUniformMatrix4x3fv)

// getUniform(program, location): find the uniform's type by scanning the
// active uniforms (matching locations, including array elements), then read
// back with the appropriately-typed glGetUniform*.
FN(w_get_uniform) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint program = obj_id(info[0]);
	GLObj *lo = get_gl_obj(info[1]);
	if (!lo)
		return;
	GLint loc = lo->loc;
	GLint count = 0;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
	GLenum type = 0;
	char name[256];
	for (GLint i = 0; i < count && type == 0; i++) {
		GLsizei len = 0;
		GLint size = 0;
		GLenum t = 0;
		glGetActiveUniform(program, i, sizeof(name), &len, &size, &t, name);
		if (len > 3 && strcmp(name + len - 3, "[0]") == 0)
			name[len - 3] = '\0';
		for (GLint e = 0; e < size; e++) {
			char ename[300];
			if (e == 0)
				snprintf(ename, sizeof(ename), "%s", name);
			else
				snprintf(ename, sizeof(ename), "%s[%d]", name, e);
			if (glGetUniformLocation(program, ename) == loc) {
				type = t;
				break;
			}
		}
	}
	if (type == 0)
		return;
	// Component count + base type per uniform type.
	int comps = 1;
	enum { BT_FLOAT, BT_INT, BT_UINT, BT_BOOL } bt = BT_INT;
	switch (type) {
	case GL_FLOAT: comps = 1; bt = BT_FLOAT; break;
	case GL_FLOAT_VEC2: comps = 2; bt = BT_FLOAT; break;
	case GL_FLOAT_VEC3: comps = 3; bt = BT_FLOAT; break;
	case GL_FLOAT_VEC4: comps = 4; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT2: comps = 4; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT3: comps = 9; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT4: comps = 16; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT2x3: comps = 6; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT2x4: comps = 8; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT3x2: comps = 6; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT3x4: comps = 12; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT4x2: comps = 8; bt = BT_FLOAT; break;
	case GL_FLOAT_MAT4x3: comps = 12; bt = BT_FLOAT; break;
	case GL_INT: case GL_SAMPLER_2D: case GL_SAMPLER_3D:
	case GL_SAMPLER_CUBE: case GL_SAMPLER_2D_SHADOW:
	case GL_SAMPLER_2D_ARRAY: case GL_SAMPLER_2D_ARRAY_SHADOW:
	case GL_SAMPLER_CUBE_SHADOW: case GL_INT_SAMPLER_2D:
	case GL_INT_SAMPLER_3D: case GL_INT_SAMPLER_CUBE:
	case GL_INT_SAMPLER_2D_ARRAY: case GL_UNSIGNED_INT_SAMPLER_2D:
	case GL_UNSIGNED_INT_SAMPLER_3D: case GL_UNSIGNED_INT_SAMPLER_CUBE:
	case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
		comps = 1; bt = BT_INT; break;
	case GL_INT_VEC2: comps = 2; bt = BT_INT; break;
	case GL_INT_VEC3: comps = 3; bt = BT_INT; break;
	case GL_INT_VEC4: comps = 4; bt = BT_INT; break;
	case GL_UNSIGNED_INT: comps = 1; bt = BT_UINT; break;
	case GL_UNSIGNED_INT_VEC2: comps = 2; bt = BT_UINT; break;
	case GL_UNSIGNED_INT_VEC3: comps = 3; bt = BT_UINT; break;
	case GL_UNSIGNED_INT_VEC4: comps = 4; bt = BT_UINT; break;
	case GL_BOOL: comps = 1; bt = BT_BOOL; break;
	case GL_BOOL_VEC2: comps = 2; bt = BT_BOOL; break;
	case GL_BOOL_VEC3: comps = 3; bt = BT_BOOL; break;
	case GL_BOOL_VEC4: comps = 4; bt = BT_BOOL; break;
	default:
		return;
	}
	if (bt == BT_FLOAT) {
		float v[16] = {0};
		glGetUniformfv(program, loc, v);
		if (comps == 1)
			info.GetReturnValue().Set((double)v[0]);
		else
			info.GetReturnValue().Set(make_f32_array(iso, v, comps));
	} else if (bt == BT_UINT) {
		uint32_t v[4] = {0};
		glGetUniformuiv(program, loc, v);
		if (comps == 1)
			info.GetReturnValue().Set(v[0]);
		else
			info.GetReturnValue().Set(make_u32_array(iso, v, comps));
	} else {
		int32_t v[4] = {0};
		glGetUniformiv(program, loc, v);
		if (bt == BT_BOOL) {
			if (comps == 1) {
				info.GetReturnValue().Set(v[0] != 0);
			} else {
				Local<Context> ctx = cur(iso);
				Local<Array> arr = Array::New(iso, comps);
				for (int i = 0; i < comps; i++)
					arr->Set(ctx, i, Boolean::New(iso, v[i] != 0)).Check();
				info.GetReturnValue().Set(arr);
			}
		} else if (comps == 1) {
			info.GetReturnValue().Set(v[0]);
		} else {
			info.GetReturnValue().Set(make_i32_array(iso, v, comps));
		}
	}
}

// ---------------------------------------------------------------------------
// Vertex attributes
// ---------------------------------------------------------------------------

FN(w_enable_vertex_attrib_array) {
	GUARD();
	glEnableVertexAttribArray(a_u32(info, 0));
}
FN(w_disable_vertex_attrib_array) {
	GUARD();
	glDisableVertexAttribArray(a_u32(info, 0));
}
FN(w_vertex_attrib_pointer) {
	GUARD();
	glVertexAttribPointer(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	                      a_bool(info, 3), a_i32(info, 4),
	                      (const void *)(uintptr_t)a_i64(info, 5));
}
FN(w_vertex_attrib_i_pointer) {
	GUARD();
	glVertexAttribIPointer(a_u32(info, 0), a_i32(info, 1), a_u32(info, 2),
	                       a_i32(info, 3),
	                       (const void *)(uintptr_t)a_i64(info, 4));
}
FN(w_vertex_attrib_divisor) {
	GUARD();
	glVertexAttribDivisor(a_u32(info, 0), a_u32(info, 1));
}
FN(w_vertex_attrib1f) {
	GUARD();
	glVertexAttrib1f(a_u32(info, 0), a_f32(info, 1));
}
FN(w_vertex_attrib2f) {
	GUARD();
	glVertexAttrib2f(a_u32(info, 0), a_f32(info, 1), a_f32(info, 2));
}
FN(w_vertex_attrib3f) {
	GUARD();
	glVertexAttrib3f(a_u32(info, 0), a_f32(info, 1), a_f32(info, 2),
	                 a_f32(info, 3));
}
FN(w_vertex_attrib4f) {
	GUARD();
	glVertexAttrib4f(a_u32(info, 0), a_f32(info, 1), a_f32(info, 2),
	                 a_f32(info, 3), a_f32(info, 4));
}
FN(w_vertex_attrib_i4i) {
	GUARD();
	glVertexAttribI4i(a_u32(info, 0), a_i32(info, 1), a_i32(info, 2),
	                  a_i32(info, 3), a_i32(info, 4));
}
FN(w_vertex_attrib_i4ui) {
	GUARD();
	glVertexAttribI4ui(a_u32(info, 0), a_u32(info, 1), a_u32(info, 2),
	                   a_u32(info, 3), a_u32(info, 4));
}

#define VATTR_VEC(NAME, N, TYPE, GETLIST, GLCALL)                              \
	FN(NAME) {                                                                 \
		GUARD();                                                               \
		Isolate *iso = info.GetIsolate();                                      \
		std::vector<TYPE> tmp;                                                 \
		const TYPE *d;                                                         \
		size_t n;                                                              \
		if (!GETLIST(iso, info[1], tmp, &d, &n, 0, 0))                         \
			return;                                                            \
		if (n < N) {                                                           \
			record_error(GL_INVALID_VALUE);                                    \
			return;                                                            \
		}                                                                      \
		GLCALL(a_u32(info, 0), d);                                             \
	}

VATTR_VEC(w_vertex_attrib1fv, 1, float, get_f32_list, glVertexAttrib1fv)
VATTR_VEC(w_vertex_attrib2fv, 2, float, get_f32_list, glVertexAttrib2fv)
VATTR_VEC(w_vertex_attrib3fv, 3, float, get_f32_list, glVertexAttrib3fv)
VATTR_VEC(w_vertex_attrib4fv, 4, float, get_f32_list, glVertexAttrib4fv)
VATTR_VEC(w_vertex_attrib_i4iv, 4, int32_t, get_i32_list, glVertexAttribI4iv)
VATTR_VEC(w_vertex_attrib_i4uiv, 4, uint32_t, get_u32_list,
          glVertexAttribI4uiv)

FN(w_get_vertex_attrib) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint index = a_u32(info, 0);
	GLenum pname = a_u32(info, 1);
	switch (pname) {
	case GL_CURRENT_VERTEX_ATTRIB: {
		float v[4] = {0};
		glGetVertexAttribfv(index, pname, v);
		info.GetReturnValue().Set(make_f32_array(iso, v, 4));
		return;
	}
	case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
		return; // object identity not tracked; null
	case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
	case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
	case GL_VERTEX_ATTRIB_ARRAY_INTEGER: {
		GLint v = 0;
		glGetVertexAttribiv(index, pname, &v);
		info.GetReturnValue().Set(v == GL_TRUE);
		return;
	}
	}
	GLint v = 0;
	glGetVertexAttribiv(index, pname, &v);
	info.GetReturnValue().Set(v);
}
FN(w_get_vertex_attrib_offset) {
	info.GetReturnValue().Set(0);
	GUARD();
	void *ptr = nullptr;
	glGetVertexAttribPointerv(a_u32(info, 0), a_u32(info, 1), &ptr);
	info.GetReturnValue().Set((double)(uintptr_t)ptr);
}

// ---------------------------------------------------------------------------
// Queries, samplers, sync, transform feedback, uniform blocks
// ---------------------------------------------------------------------------

FN(w_begin_query) { GUARD(); glBeginQuery(a_u32(info, 0), obj_id(info[1])); }
FN(w_end_query) { GUARD(); glEndQuery(a_u32(info, 0)); }
FN(w_get_query) {
	// Active query object identity is not tracked; null.
	info.GetReturnValue().SetNull();
}
FN(w_get_query_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLuint query = obj_id(info[0]);
	GLenum pname = a_u32(info, 1);
	GLuint v = 0;
	glGetQueryObjectuiv(query, pname, &v);
	if (pname == GL_QUERY_RESULT_AVAILABLE)
		info.GetReturnValue().Set(v == GL_TRUE);
	else
		info.GetReturnValue().Set(v);
}
FN(w_sampler_parameteri) {
	GUARD();
	glSamplerParameteri(obj_id(info[0]), a_u32(info, 1), a_i32(info, 2));
}
FN(w_sampler_parameterf) {
	GUARD();
	glSamplerParameterf(obj_id(info[0]), a_u32(info, 1), a_f32(info, 2));
}
FN(w_get_sampler_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLuint sampler = obj_id(info[0]);
	GLenum pname = a_u32(info, 1);
	if (pname == GL_TEXTURE_MIN_LOD || pname == GL_TEXTURE_MAX_LOD) {
		GLfloat f = 0;
		glGetSamplerParameterfv(sampler, pname, &f);
		info.GetReturnValue().Set((double)f);
		return;
	}
	GLint v = 0;
	glGetSamplerParameteriv(sampler, pname, &v);
	info.GetReturnValue().Set(v);
}

FN(w_fence_sync) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLsync sync = glFenceSync(a_u32(info, 0), a_u32(info, 1));
	if (!sync)
		return;
	info.GetReturnValue().Set(
	    new_gl_obj(info.GetIsolate(), K_SYNC, 0, sync));
}
FN(w_is_sync) {
	info.GetReturnValue().Set(false);
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || !o->sync)
		return;
	info.GetReturnValue().Set(glIsSync(o->sync) == GL_TRUE);
}
FN(w_delete_sync) {
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || !o->sync)
		return;
	glDeleteSync(o->sync);
	o->sync = nullptr;
}
FN(w_client_wait_sync) {
	info.GetReturnValue().Set((uint32_t)GL_WAIT_FAILED);
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || !o->sync)
		return;
	GLuint64 timeout = (GLuint64)a_f64(info, 2);
	info.GetReturnValue().Set(
	    (uint32_t)glClientWaitSync(o->sync, a_u32(info, 1), timeout));
}
FN(w_wait_sync) {
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || !o->sync)
		return;
	glWaitSync(o->sync, a_u32(info, 1), GL_TIMEOUT_IGNORED);
}
FN(w_get_sync_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLObj *o = get_gl_obj(info[0]);
	if (!o || !o->sync)
		return;
	GLint v = 0;
	GLsizei len = 0;
	glGetSynciv(o->sync, a_u32(info, 1), 1, &len, &v);
	info.GetReturnValue().Set(v);
}

FN(w_begin_transform_feedback) {
	GUARD();
	glBeginTransformFeedback(a_u32(info, 0));
}
FN(w_end_transform_feedback) { GUARD(); glEndTransformFeedback(); }
FN(w_pause_transform_feedback) { GUARD(); glPauseTransformFeedback(); }
FN(w_resume_transform_feedback) { GUARD(); glResumeTransformFeedback(); }
FN(w_transform_feedback_varyings) {
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	if (!info[1]->IsArray())
		return;
	Local<Array> arr = info[1].As<Array>();
	uint32_t n = arr->Length();
	std::vector<std::string> strs(n);
	std::vector<const char *> ptrs(n);
	for (uint32_t i = 0; i < n; i++) {
		Local<Value> el;
		if (arr->Get(ctx, i).ToLocal(&el)) {
			String::Utf8Value s(iso, el);
			if (*s)
				strs[i] = *s;
		}
		ptrs[i] = strs[i].c_str();
	}
	glTransformFeedbackVaryings(obj_id(info[0]), (GLsizei)n, ptrs.data(),
	                            a_u32(info, 2));
}
FN(w_get_transform_feedback_varying) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	char name[256];
	GLsizei len = 0;
	GLsizei size = 0;
	GLenum type = 0;
	glGetTransformFeedbackVarying(obj_id(info[0]), a_u32(info, 1),
	                              sizeof(name), &len, &size, &type, name);
	if (type == 0)
		return;
	Local<Object> o = Object::New(iso);
	if (!st->protos[K_ACTIVE_INFO].IsEmpty()) {
		o->SetPrototype(ctx, st->protos[K_ACTIVE_INFO].Get(iso)).Check();
	}
	o->Set(ctx, nx_str(iso, "name"), nx_str_lossy(iso, name, len)).Check();
	o->Set(ctx, nx_str(iso, "size"), Integer::New(iso, size)).Check();
	o->Set(ctx, nx_str(iso, "type"), Integer::NewFromUnsigned(iso, type))
	    .Check();
	info.GetReturnValue().Set(o);
}

FN(w_get_uniform_indices) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	if (!info[1]->IsArray())
		return;
	Local<Array> arr = info[1].As<Array>();
	uint32_t n = arr->Length();
	std::vector<std::string> strs(n);
	std::vector<const char *> ptrs(n);
	for (uint32_t i = 0; i < n; i++) {
		Local<Value> el;
		if (arr->Get(ctx, i).ToLocal(&el)) {
			String::Utf8Value s(iso, el);
			if (*s)
				strs[i] = *s;
		}
		ptrs[i] = strs[i].c_str();
	}
	std::vector<GLuint> indices(n);
	glGetUniformIndices(obj_id(info[0]), (GLsizei)n, ptrs.data(),
	                    indices.data());
	Local<Array> out = Array::New(iso, n);
	for (uint32_t i = 0; i < n; i++)
		out->Set(ctx, i, Integer::NewFromUnsigned(iso, indices[i])).Check();
	info.GetReturnValue().Set(out);
}
FN(w_get_active_uniforms) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	if (!info[1]->IsArray())
		return;
	Local<Array> arr = info[1].As<Array>();
	GLenum pname = a_u32(info, 2);
	uint32_t n = arr->Length();
	std::vector<GLuint> indices(n);
	for (uint32_t i = 0; i < n; i++) {
		Local<Value> el;
		indices[i] = 0;
		if (arr->Get(ctx, i).ToLocal(&el))
			indices[i] = el->Uint32Value(ctx).FromMaybe(0);
	}
	std::vector<GLint> params(n);
	glGetActiveUniformsiv(obj_id(info[0]), (GLsizei)n, indices.data(), pname,
	                      params.data());
	Local<Array> out = Array::New(iso, n);
	bool is_bool = pname == GL_UNIFORM_IS_ROW_MAJOR;
	for (uint32_t i = 0; i < n; i++) {
		if (is_bool)
			out->Set(ctx, i, Boolean::New(iso, params[i] != 0)).Check();
		else
			out->Set(ctx, i, Integer::New(iso, params[i])).Check();
	}
	info.GetReturnValue().Set(out);
}
FN(w_get_uniform_block_index) {
	info.GetReturnValue().Set((uint32_t)GL_INVALID_INDEX);
	GUARD();
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[1]);
	if (!*name)
		return;
	info.GetReturnValue().Set(
	    (uint32_t)glGetUniformBlockIndex(obj_id(info[0]), *name));
}
FN(w_get_active_uniform_block_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLuint program = obj_id(info[0]);
	GLuint index = a_u32(info, 1);
	GLenum pname = a_u32(info, 2);
	switch (pname) {
	case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: {
		GLint count = 0;
		glGetActiveUniformBlockiv(program, index,
		                          GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &count);
		std::vector<GLint> v((size_t)count);
		if (count > 0)
			glGetActiveUniformBlockiv(program, index, pname, v.data());
		info.GetReturnValue().Set(
		    make_u32_array(iso, (const uint32_t *)v.data(), v.size()));
		return;
	}
	case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
	case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: {
		GLint v = 0;
		glGetActiveUniformBlockiv(program, index, pname, &v);
		info.GetReturnValue().Set(v == GL_TRUE);
		return;
	}
	}
	GLint v = 0;
	glGetActiveUniformBlockiv(program, index, pname, &v);
	info.GetReturnValue().Set(v);
}
FN(w_get_active_uniform_block_name) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	char name[256];
	GLsizei len = 0;
	glGetActiveUniformBlockName(obj_id(info[0]), a_u32(info, 1), sizeof(name),
	                            &len, name);
	info.GetReturnValue().Set(nx_str_lossy(iso, name, len));
}
FN(w_uniform_block_binding) {
	GUARD();
	glUniformBlockBinding(obj_id(info[0]), a_u32(info, 1), a_u32(info, 2));
}

FN(w_get_indexed_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	GLenum target = a_u32(info, 0);
	GLuint index = a_u32(info, 1);
	switch (target) {
	case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
	case GL_UNIFORM_BUFFER_BINDING:
		return; // object identity not tracked; null
	case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
	case GL_TRANSFORM_FEEDBACK_BUFFER_START:
	case GL_UNIFORM_BUFFER_SIZE:
	case GL_UNIFORM_BUFFER_START: {
		GLint64 v = 0;
		glGetInteger64i_v(target, index, &v);
		info.GetReturnValue().Set((double)v);
		return;
	}
	}
	GLint v = 0;
	glGetIntegeri_v(target, index, &v);
	info.GetReturnValue().Set(v);
}

// ---------------------------------------------------------------------------
// getParameter
// ---------------------------------------------------------------------------

FN(w_get_parameter) {
	info.GetReturnValue().SetNull();
	GUARD();
	Isolate *iso = info.GetIsolate();
	GLenum pname = a_u32(info, 0);
	switch (pname) {
	// ---- strings ----
	case GL_VENDOR:
	case GL_RENDERER: {
		const char *s = (const char *)glGetString(pname);
		info.GetReturnValue().Set(nx_str_lossy(iso, s ? s : ""));
		return;
	}
	case GL_VERSION: {
		const char *s = (const char *)glGetString(GL_VERSION);
		std::string v = "WebGL 2.0 (";
		v += s ? s : "unknown";
		v += ")";
		info.GetReturnValue().Set(nx_str_lossy(iso, v.c_str()));
		return;
	}
	case GL_SHADING_LANGUAGE_VERSION: {
		const char *s = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
		std::string v = "WebGL GLSL ES 3.00 (";
		v += s ? s : "unknown";
		v += ")";
		info.GetReturnValue().Set(nx_str_lossy(iso, v.c_str()));
		return;
	}
	// ---- single floats ----
	case GL_DEPTH_CLEAR_VALUE:
	case GL_LINE_WIDTH:
	case GL_POLYGON_OFFSET_FACTOR:
	case GL_POLYGON_OFFSET_UNITS:
	case GL_SAMPLE_COVERAGE_VALUE:
	case GL_MAX_TEXTURE_LOD_BIAS: {
		GLfloat f = 0;
		glGetFloatv(pname, &f);
		info.GetReturnValue().Set((double)f);
		return;
	}
	// ---- Float32Array(2) ----
	case GL_ALIASED_LINE_WIDTH_RANGE:
	case GL_ALIASED_POINT_SIZE_RANGE:
	case GL_DEPTH_RANGE: {
		float v[2] = {0};
		glGetFloatv(pname, v);
		info.GetReturnValue().Set(make_f32_array(iso, v, 2));
		return;
	}
	// ---- Float32Array(4) ----
	case GL_BLEND_COLOR:
	case GL_COLOR_CLEAR_VALUE: {
		float v[4] = {0};
		glGetFloatv(pname, v);
		info.GetReturnValue().Set(make_f32_array(iso, v, 4));
		return;
	}
	// ---- Int32Array(2) ----
	case GL_MAX_VIEWPORT_DIMS: {
		GLint v[2] = {0};
		glGetIntegerv(pname, v);
		info.GetReturnValue().Set(make_i32_array(iso, v, 2));
		return;
	}
	// ---- Int32Array(4) ----
	case GL_SCISSOR_BOX:
	case GL_VIEWPORT: {
		GLint v[4] = {0};
		glGetIntegerv(pname, v);
		info.GetReturnValue().Set(make_i32_array(iso, v, 4));
		return;
	}
	// ---- booleans ----
	case GL_BLEND:
	case GL_CULL_FACE:
	case GL_DEPTH_TEST:
	case GL_DEPTH_WRITEMASK:
	case GL_DITHER:
	case GL_POLYGON_OFFSET_FILL:
	case GL_SAMPLE_ALPHA_TO_COVERAGE:
	case GL_SAMPLE_COVERAGE_INVERT:
	case GL_SCISSOR_TEST:
	case GL_STENCIL_TEST:
	case GL_RASTERIZER_DISCARD:
	case GL_TRANSFORM_FEEDBACK_ACTIVE:
	case GL_TRANSFORM_FEEDBACK_PAUSED: {
		GLboolean b = GL_FALSE;
		glGetBooleanv(pname, &b);
		info.GetReturnValue().Set(b == GL_TRUE);
		return;
	}
	// ---- bool[4] ----
	case GL_COLOR_WRITEMASK: {
		GLboolean v[4] = {0};
		glGetBooleanv(pname, v);
		Local<Context> ctx = cur(iso);
		Local<Array> arr = Array::New(iso, 4);
		for (int i = 0; i < 4; i++)
			arr->Set(ctx, i, Boolean::New(iso, v[i] == GL_TRUE)).Check();
		info.GetReturnValue().Set(arr);
		return;
	}
	// ---- int64 ----
	case GL_MAX_ELEMENT_INDEX:
	case GL_MAX_SERVER_WAIT_TIMEOUT:
	case GL_MAX_UNIFORM_BLOCK_SIZE:
	case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
	case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: {
		GLint64 v = 0;
		glGetInteger64v(pname, &v);
		info.GetReturnValue().Set((double)v);
		return;
	}
	// ---- WebGL-specific ----
	case NX_GL_UNPACK_FLIP_Y_WEBGL:
		info.GetReturnValue().Set(st->unpack_flip_y);
		return;
	case NX_GL_UNPACK_PREMULTIPLY_ALPHA_WEBGL:
		info.GetReturnValue().Set(st->unpack_premultiply);
		return;
	case NX_GL_UNPACK_COLORSPACE_CONVERSION_WEBGL:
		info.GetReturnValue().Set(0x9244); // BROWSER_DEFAULT_WEBGL
		return;
	case 0x9245: // MAX_CLIENT_WAIT_TIMEOUT_WEBGL (EXT_disjoint... not used)
		info.GetReturnValue().Set(0);
		return;
	// ---- object bindings (identity not tracked; null) ----
	case GL_ARRAY_BUFFER_BINDING:
	case GL_ELEMENT_ARRAY_BUFFER_BINDING:
	case GL_COPY_READ_BUFFER_BINDING:
	case GL_COPY_WRITE_BUFFER_BINDING:
	case GL_PIXEL_PACK_BUFFER_BINDING:
	case GL_PIXEL_UNPACK_BUFFER_BINDING:
	case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
	case GL_UNIFORM_BUFFER_BINDING:
	case GL_CURRENT_PROGRAM:
	case GL_FRAMEBUFFER_BINDING: // == DRAW_FRAMEBUFFER_BINDING
	case GL_READ_FRAMEBUFFER_BINDING:
	case GL_RENDERBUFFER_BINDING:
	case GL_TEXTURE_BINDING_2D:
	case GL_TEXTURE_BINDING_CUBE_MAP:
	case GL_TEXTURE_BINDING_3D:
	case GL_TEXTURE_BINDING_2D_ARRAY:
	case GL_SAMPLER_BINDING:
	case GL_TRANSFORM_FEEDBACK_BINDING:
	case GL_VERTEX_ARRAY_BINDING:
		return;
	}
	// Default: single integer (MAX_* limits, enum-valued state, etc).
	GLint v = 0;
	glGetIntegerv(pname, &v);
	info.GetReturnValue().Set(v);
}

// ---------------------------------------------------------------------------
// Context creation / initClass / present / teardown
// ---------------------------------------------------------------------------

bool init_egl_es3(NWindow *win) {
	st->dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!st->dpy)
		return false;
	eglInitialize(st->dpy, nullptr, nullptr);
	eglBindAPI(EGL_OPENGL_ES_API);
	EGLConfig cfg;
	EGLint num = 0;
	const EGLint attrs[] = {EGL_RENDERABLE_TYPE,
	                        0x0040, // EGL_OPENGL_ES3_BIT
	                        EGL_RED_SIZE,
	                        8,
	                        EGL_GREEN_SIZE,
	                        8,
	                        EGL_BLUE_SIZE,
	                        8,
	                        EGL_ALPHA_SIZE,
	                        8,
	                        EGL_DEPTH_SIZE,
	                        24,
	                        EGL_STENCIL_SIZE,
	                        8,
	                        EGL_NONE};
	eglChooseConfig(st->dpy, attrs, &cfg, 1, &num);
	if (!num) {
		fprintf(stderr, "[webgl] eglChooseConfig: no ES3 config\n");
		return false;
	}
	st->surf = eglCreateWindowSurface(st->dpy, cfg, win, nullptr);
	if (!st->surf) {
		fprintf(stderr, "[webgl] eglCreateWindowSurface failed: 0x%x\n",
		        eglGetError());
		return false;
	}
	const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	st->ctx = eglCreateContext(st->dpy, cfg, EGL_NO_CONTEXT, ca);
	if (!st->ctx) {
		fprintf(stderr, "[webgl] eglCreateContext (ES3) failed: 0x%x\n",
		        eglGetError());
		return false;
	}
	if (eglMakeCurrent(st->dpy, st->surf, st->surf, st->ctx) != EGL_TRUE) {
		fprintf(stderr, "[webgl] eglMakeCurrent failed: 0x%x\n", eglGetError());
		return false;
	}
	// One swap per vblank (same policy as the Skia GPU path).
	if (eglSwapInterval(st->dpy, 1) != EGL_TRUE) {
		fprintf(stderr, "[webgl] eglSwapInterval(1) failed: 0x%x\n",
		        eglGetError());
	}
	return true;
}

void webgl_teardown_egl() {
	if (st && st->dpy) {
		eglMakeCurrent(st->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (st->ctx) {
			eglDestroyContext(st->dpy, st->ctx);
			st->ctx = EGL_NO_CONTEXT;
		}
		if (st->surf) {
			eglDestroySurface(st->dpy, st->surf);
			st->surf = EGL_NO_SURFACE;
		}
		eglTerminate(st->dpy);
		st->dpy = EGL_NO_DISPLAY;
	}
	if (st)
		st->active = false;
}

// $.webglContextNew(screen): bring up EGL + an OpenGL ES 3 context on the
// default NWindow. Returns the context carrier object, or undefined when GL
// init fails (the TS side then returns null from getContext).
FN(nx_webgl_context_new) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);
	if (!st)
		st = new WebGLState();
	if (st->active)
		return; // already created (TS guards against this)
	// Release whatever currently owns the display path (PrintConsole, raster
	// framebuffer, or a console-initialized Skia GPU screen) before EGL
	// claims the NWindow.
	nx_screen_release_for_webgl(iso, info[0]);
	if (!init_egl_es3(nwindowGetDefault())) {
		webgl_teardown_egl();
		fprintf(stderr, "[webgl] EGL/ES3 init failed; getContext('webgl2') "
		                "returns null\n");
		fflush(stderr);
		return;
	}
	const char *version = (const char *)glGetString(GL_VERSION);
	if (!version || strncmp(version, "OpenGL ES 3.", 12) != 0) {
		fprintf(stderr, "[webgl] driver is not ES3 (%s)\n",
		        version ? version : "null");
		fflush(stderr);
		webgl_teardown_egl();
		return;
	}
	// Drawing buffer dimensions: Mesa/nouveau's eglQuerySurface reports 0x0
	// for the NWindow surface, but the spec-mandated *initial GL viewport*
	// equals the surface size — read that instead, with EGL as a secondary
	// source and the NWindow default (1280x720) as a last resort.
	GLint vp[4] = {0, 0, 0, 0};
	glGetIntegerv(GL_VIEWPORT, vp);
	EGLint w = vp[2], h = vp[3];
	if (w <= 0 || h <= 0) {
		eglQuerySurface(st->dpy, st->surf, EGL_WIDTH, &w);
		eglQuerySurface(st->dpy, st->surf, EGL_HEIGHT, &h);
	}
	if (w <= 0 || h <= 0) {
		w = 1280;
		h = 720;
	}
	st->width = w;
	st->height = h;
	st->active = true;
	st->dirty = false;
	st->draw_fbo = 0;
	st->synthetic_error = GL_NO_ERROR;
	st->unpack_flip_y = false;
	st->unpack_premultiply = false;
	st->unpack_alignment = 4;
	st->unpack_row_length = 0;
	st->pack_alignment = 4;
	// Clear both swapchain buffers so no garbage is ever presented.
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	eglSwapBuffers(st->dpy, st->surf);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	fprintf(stderr, "[webgl] ES3 context ready: %dx%d (%s)\n", w, h, version);
	fflush(stderr);
	// Bringing up EGL/Mesa can leave HID not delivering input when it runs
	// after the pads were configured (same as the Skia GPU path in
	// nx_framebuffer_init); re-configure + re-initialize the pads.
	padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc);
	padInitializeDefault(&ctx->pads[0]);
	for (int i = 1; i < 8; i++) {
		padInitialize(&ctx->pads[i], (HidNpadIdType)(HidNpadIdType_No1 + i));
	}
	// The main loop's CANVAS present branch dispatches to nx_webgl_present()
	// when a WebGL context is active.
	ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
	info.GetReturnValue().Set(nx::NewWrapped(iso));
}

FN(w_drawing_buffer_width) {
	info.GetReturnValue().Set(st ? st->width : 0);
}
FN(w_drawing_buffer_height) {
	info.GetReturnValue().Set(st ? st->height : 0);
}

// $.webglInitClass(WebGL2RenderingContext, { WebGLBuffer, ... }): stamp the
// native methods onto the context prototype + retain the WebGL object class
// prototypes so C-minted objects get correct `instanceof` behavior.
FN(nx_webgl_init_class) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = cur(iso);
	if (!st)
		st = new WebGLState();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(ctx, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	static const struct {
		const char *name;
		ObjKind kind;
	} kClasses[] = {
	    {"WebGLBuffer", K_BUFFER},
	    {"WebGLFramebuffer", K_FRAMEBUFFER},
	    {"WebGLProgram", K_PROGRAM},
	    {"WebGLQuery", K_QUERY},
	    {"WebGLRenderbuffer", K_RENDERBUFFER},
	    {"WebGLSampler", K_SAMPLER},
	    {"WebGLShader", K_SHADER},
	    {"WebGLTexture", K_TEXTURE},
	    {"WebGLTransformFeedback", K_TRANSFORM_FEEDBACK},
	    {"WebGLVertexArrayObject", K_VERTEX_ARRAY},
	    {"WebGLSync", K_SYNC},
	    {"WebGLUniformLocation", K_UNIFORM_LOCATION},
	    {"WebGLActiveInfo", K_ACTIVE_INFO},
	    {"WebGLShaderPrecisionFormat", K_SHADER_PRECISION_FORMAT},
	};
	if (info.Length() > 1 && info[1]->IsObject()) {
		Local<Object> map = info[1].As<Object>();
		for (size_t i = 0; i < countof(kClasses); i++) {
			Local<Value> cls;
			if (!map->Get(ctx, nx_str(iso, kClasses[i].name)).ToLocal(&cls) ||
			    !cls->IsObject())
				continue;
			Local<Value> p;
			if (!cls.As<Object>()
			         ->Get(ctx, nx_str(iso, "prototype"))
			         .ToLocal(&p) ||
			    !p->IsObject())
				continue;
			st->protos[kClasses[i].kind].Reset(iso, p.As<Object>());
		}
	}
	NX_DEF_GET(proto, "drawingBufferWidth", w_drawing_buffer_width);
	NX_DEF_GET(proto, "drawingBufferHeight", w_drawing_buffer_height);
	static const struct {
		const char *name;
		FunctionCallback fn;
		int len;
	} kFns[] = {
	    {"activeTexture", w_active_texture, 1},
	    {"attachShader", w_attach_shader, 2},
	    {"beginQuery", w_begin_query, 2},
	    {"beginTransformFeedback", w_begin_transform_feedback, 1},
	    {"bindAttribLocation", w_bind_attrib_location, 3},
	    {"bindBuffer", w_bind_buffer, 2},
	    {"bindBufferBase", w_bind_buffer_base, 3},
	    {"bindBufferRange", w_bind_buffer_range, 5},
	    {"bindFramebuffer", w_bind_framebuffer, 2},
	    {"bindRenderbuffer", w_bind_renderbuffer, 2},
	    {"bindSampler", w_bind_sampler, 2},
	    {"bindTexture", w_bind_texture, 2},
	    {"bindTransformFeedback", w_bind_transform_feedback, 2},
	    {"bindVertexArray", w_bind_vertex_array, 1},
	    {"blendColor", w_blend_color, 4},
	    {"blendEquation", w_blend_equation, 1},
	    {"blendEquationSeparate", w_blend_equation_separate, 2},
	    {"blendFunc", w_blend_func, 2},
	    {"blendFuncSeparate", w_blend_func_separate, 4},
	    {"blitFramebuffer", w_blit_framebuffer, 10},
	    {"bufferData", w_buffer_data, 3},
	    {"bufferSubData", w_buffer_sub_data, 3},
	    {"checkFramebufferStatus", w_check_framebuffer_status, 1},
	    {"clear", w_clear, 1},
	    {"clearBufferfi", w_clear_bufferfi, 4},
	    {"clearBufferfv", w_clear_bufferfv, 3},
	    {"clearBufferiv", w_clear_bufferiv, 3},
	    {"clearBufferuiv", w_clear_bufferuiv, 3},
	    {"clearColor", w_clear_color, 4},
	    {"clearDepth", w_clear_depth, 1},
	    {"clearStencil", w_clear_stencil, 1},
	    {"clientWaitSync", w_client_wait_sync, 3},
	    {"colorMask", w_color_mask, 4},
	    {"compileShader", w_compile_shader, 1},
	    {"compressedTexImage2D", w_compressed_tex_image_2d, 7},
	    {"compressedTexImage3D", w_compressed_tex_image_3d, 8},
	    {"compressedTexSubImage2D", w_compressed_tex_sub_image_2d, 8},
	    {"compressedTexSubImage3D", w_compressed_tex_sub_image_3d, 10},
	    {"copyBufferSubData", w_copy_buffer_sub_data, 5},
	    {"copyTexImage2D", w_copy_tex_image_2d, 8},
	    {"copyTexSubImage2D", w_copy_tex_sub_image_2d, 8},
	    {"copyTexSubImage3D", w_copy_tex_sub_image_3d, 9},
	    {"createBuffer", w_create_buffer, 0},
	    {"createFramebuffer", w_create_framebuffer, 0},
	    {"createProgram", w_create_program, 0},
	    {"createQuery", w_create_query, 0},
	    {"createRenderbuffer", w_create_renderbuffer, 0},
	    {"createSampler", w_create_sampler, 0},
	    {"createShader", w_create_shader, 1},
	    {"createTexture", w_create_texture, 0},
	    {"createTransformFeedback", w_create_transform_feedback, 0},
	    {"createVertexArray", w_create_vertex_array, 0},
	    {"cullFace", w_cull_face, 1},
	    {"deleteBuffer", w_delete_buffer, 1},
	    {"deleteFramebuffer", w_delete_framebuffer, 1},
	    {"deleteProgram", w_delete_program, 1},
	    {"deleteQuery", w_delete_query, 1},
	    {"deleteRenderbuffer", w_delete_renderbuffer, 1},
	    {"deleteSampler", w_delete_sampler, 1},
	    {"deleteShader", w_delete_shader, 1},
	    {"deleteSync", w_delete_sync, 1},
	    {"deleteTexture", w_delete_texture, 1},
	    {"deleteTransformFeedback", w_delete_transform_feedback, 1},
	    {"deleteVertexArray", w_delete_vertex_array, 1},
	    {"depthFunc", w_depth_func, 1},
	    {"depthMask", w_depth_mask, 1},
	    {"depthRange", w_depth_range, 2},
	    {"detachShader", w_detach_shader, 2},
	    {"disable", w_disable, 1},
	    {"disableVertexAttribArray", w_disable_vertex_attrib_array, 1},
	    {"drawArrays", w_draw_arrays, 3},
	    {"drawArraysInstanced", w_draw_arrays_instanced, 4},
	    {"drawBuffers", w_draw_buffers, 1},
	    {"drawElements", w_draw_elements, 4},
	    {"drawElementsInstanced", w_draw_elements_instanced, 5},
	    {"drawRangeElements", w_draw_range_elements, 6},
	    {"enable", w_enable, 1},
	    {"enableVertexAttribArray", w_enable_vertex_attrib_array, 1},
	    {"endQuery", w_end_query, 1},
	    {"endTransformFeedback", w_end_transform_feedback, 0},
	    {"fenceSync", w_fence_sync, 2},
	    {"finish", w_finish, 0},
	    {"flush", w_flush, 0},
	    {"framebufferRenderbuffer", w_framebuffer_renderbuffer, 4},
	    {"framebufferTexture2D", w_framebuffer_texture_2d, 5},
	    {"framebufferTextureLayer", w_framebuffer_texture_layer, 5},
	    {"frontFace", w_front_face, 1},
	    {"generateMipmap", w_generate_mipmap, 1},
	    {"getActiveAttrib", w_get_active_attrib, 2},
	    {"getActiveUniform", w_get_active_uniform, 2},
	    {"getActiveUniformBlockName", w_get_active_uniform_block_name, 2},
	    {"getActiveUniformBlockParameter",
	     w_get_active_uniform_block_parameter, 3},
	    {"getActiveUniforms", w_get_active_uniforms, 3},
	    {"getAttachedShaders", w_get_attached_shaders, 1},
	    {"getAttribLocation", w_get_attrib_location, 2},
	    {"getBufferParameter", w_get_buffer_parameter, 2},
	    {"getBufferSubData", w_get_buffer_sub_data, 3},
	    {"getError", w_get_error, 0},
	    {"getFragDataLocation", w_get_frag_data_location, 2},
	    {"getFramebufferAttachmentParameter",
	     w_get_framebuffer_attachment_parameter, 3},
	    {"getIndexedParameter", w_get_indexed_parameter, 2},
	    {"getInternalformatParameter", w_get_internalformat_parameter, 3},
	    {"getParameter", w_get_parameter, 1},
	    {"getProgramInfoLog", w_get_program_info_log, 1},
	    {"getProgramParameter", w_get_program_parameter, 2},
	    {"getQuery", w_get_query, 2},
	    {"getQueryParameter", w_get_query_parameter, 2},
	    {"getRenderbufferParameter", w_get_renderbuffer_parameter, 2},
	    {"getSamplerParameter", w_get_sampler_parameter, 2},
	    {"getShaderInfoLog", w_get_shader_info_log, 1},
	    {"getShaderParameter", w_get_shader_parameter, 2},
	    {"getShaderPrecisionFormat", w_get_shader_precision_format, 2},
	    {"getShaderSource", w_get_shader_source, 1},
	    {"getSyncParameter", w_get_sync_parameter, 2},
	    {"getTexParameter", w_get_tex_parameter, 2},
	    {"getTransformFeedbackVarying", w_get_transform_feedback_varying, 2},
	    {"getUniform", w_get_uniform, 2},
	    {"getUniformBlockIndex", w_get_uniform_block_index, 2},
	    {"getUniformIndices", w_get_uniform_indices, 2},
	    {"getUniformLocation", w_get_uniform_location, 2},
	    {"getVertexAttrib", w_get_vertex_attrib, 2},
	    {"getVertexAttribOffset", w_get_vertex_attrib_offset, 2},
	    {"hint", w_hint, 2},
	    {"invalidateFramebuffer", w_invalidate_framebuffer, 2},
	    {"invalidateSubFramebuffer", w_invalidate_sub_framebuffer, 6},
	    {"isBuffer", w_is_buffer, 1},
	    {"isEnabled", w_is_enabled, 1},
	    {"isFramebuffer", w_is_framebuffer, 1},
	    {"isProgram", w_is_program, 1},
	    {"isQuery", w_is_query, 1},
	    {"isRenderbuffer", w_is_renderbuffer, 1},
	    {"isSampler", w_is_sampler, 1},
	    {"isShader", w_is_shader, 1},
	    {"isSync", w_is_sync, 1},
	    {"isTexture", w_is_texture, 1},
	    {"isTransformFeedback", w_is_transform_feedback, 1},
	    {"isVertexArray", w_is_vertex_array, 1},
	    {"lineWidth", w_line_width, 1},
	    {"linkProgram", w_link_program, 1},
	    {"pauseTransformFeedback", w_pause_transform_feedback, 0},
	    {"pixelStorei", w_pixel_storei, 2},
	    {"polygonOffset", w_polygon_offset, 2},
	    {"readBuffer", w_read_buffer, 1},
	    {"readPixels", w_read_pixels, 7},
	    {"renderbufferStorage", w_renderbuffer_storage, 4},
	    {"renderbufferStorageMultisample", w_renderbuffer_storage_multisample,
	     5},
	    {"resumeTransformFeedback", w_resume_transform_feedback, 0},
	    {"sampleCoverage", w_sample_coverage, 2},
	    {"samplerParameterf", w_sampler_parameterf, 3},
	    {"samplerParameteri", w_sampler_parameteri, 3},
	    {"scissor", w_scissor, 4},
	    {"shaderSource", w_shader_source, 2},
	    {"stencilFunc", w_stencil_func, 3},
	    {"stencilFuncSeparate", w_stencil_func_separate, 4},
	    {"stencilMask", w_stencil_mask, 1},
	    {"stencilMaskSeparate", w_stencil_mask_separate, 2},
	    {"stencilOp", w_stencil_op, 3},
	    {"stencilOpSeparate", w_stencil_op_separate, 4},
	    {"texImage2D", w_tex_image_2d, 9},
	    {"texImage3D", w_tex_image_3d, 10},
	    {"texParameterf", w_tex_parameterf, 3},
	    {"texParameteri", w_tex_parameteri, 3},
	    {"texStorage2D", w_tex_storage_2d, 5},
	    {"texStorage3D", w_tex_storage_3d, 6},
	    {"texSubImage2D", w_tex_sub_image_2d, 9},
	    {"texSubImage3D", w_tex_sub_image_3d, 11},
	    {"transformFeedbackVaryings", w_transform_feedback_varyings, 3},
	    {"uniform1f", w_uniform1f, 2},
	    {"uniform1fv", w_uniform1fv, 2},
	    {"uniform1i", w_uniform1i, 2},
	    {"uniform1iv", w_uniform1iv, 2},
	    {"uniform1ui", w_uniform1ui, 2},
	    {"uniform1uiv", w_uniform1uiv, 2},
	    {"uniform2f", w_uniform2f, 3},
	    {"uniform2fv", w_uniform2fv, 2},
	    {"uniform2i", w_uniform2i, 3},
	    {"uniform2iv", w_uniform2iv, 2},
	    {"uniform2ui", w_uniform2ui, 3},
	    {"uniform2uiv", w_uniform2uiv, 2},
	    {"uniform3f", w_uniform3f, 4},
	    {"uniform3fv", w_uniform3fv, 2},
	    {"uniform3i", w_uniform3i, 4},
	    {"uniform3iv", w_uniform3iv, 2},
	    {"uniform3ui", w_uniform3ui, 4},
	    {"uniform3uiv", w_uniform3uiv, 2},
	    {"uniform4f", w_uniform4f, 5},
	    {"uniform4fv", w_uniform4fv, 2},
	    {"uniform4i", w_uniform4i, 5},
	    {"uniform4iv", w_uniform4iv, 2},
	    {"uniform4ui", w_uniform4ui, 5},
	    {"uniform4uiv", w_uniform4uiv, 2},
	    {"uniformBlockBinding", w_uniform_block_binding, 3},
	    {"uniformMatrix2fv", w_uniform_matrix2fv, 3},
	    {"uniformMatrix2x3fv", w_uniform_matrix2x3fv, 3},
	    {"uniformMatrix2x4fv", w_uniform_matrix2x4fv, 3},
	    {"uniformMatrix3fv", w_uniform_matrix3fv, 3},
	    {"uniformMatrix3x2fv", w_uniform_matrix3x2fv, 3},
	    {"uniformMatrix3x4fv", w_uniform_matrix3x4fv, 3},
	    {"uniformMatrix4fv", w_uniform_matrix4fv, 3},
	    {"uniformMatrix4x2fv", w_uniform_matrix4x2fv, 3},
	    {"uniformMatrix4x3fv", w_uniform_matrix4x3fv, 3},
	    {"useProgram", w_use_program, 1},
	    {"validateProgram", w_validate_program, 1},
	    {"vertexAttrib1f", w_vertex_attrib1f, 2},
	    {"vertexAttrib1fv", w_vertex_attrib1fv, 2},
	    {"vertexAttrib2f", w_vertex_attrib2f, 3},
	    {"vertexAttrib2fv", w_vertex_attrib2fv, 2},
	    {"vertexAttrib3f", w_vertex_attrib3f, 4},
	    {"vertexAttrib3fv", w_vertex_attrib3fv, 2},
	    {"vertexAttrib4f", w_vertex_attrib4f, 5},
	    {"vertexAttrib4fv", w_vertex_attrib4fv, 2},
	    {"vertexAttribDivisor", w_vertex_attrib_divisor, 2},
	    {"vertexAttribI4i", w_vertex_attrib_i4i, 5},
	    {"vertexAttribI4iv", w_vertex_attrib_i4iv, 2},
	    {"vertexAttribI4ui", w_vertex_attrib_i4ui, 5},
	    {"vertexAttribI4uiv", w_vertex_attrib_i4uiv, 2},
	    {"vertexAttribIPointer", w_vertex_attrib_i_pointer, 5},
	    {"vertexAttribPointer", w_vertex_attrib_pointer, 6},
	    {"viewport", w_viewport, 4},
	    {"waitSync", w_wait_sync, 3},
	};
	for (size_t i = 0; i < countof(kFns); i++) {
		NX_DEF_FUNC(proto, kFns[i].name, kFns[i].fn, kFns[i].len);
	}
}

} // namespace

bool nx_webgl_active(void) { return st && st->active; }

void nx_webgl_present(void) {
	if (!st || !st->active)
		return;
	if (st->dirty) {
		eglSwapBuffers(st->dpy, st->surf);
		st->dirty = false;
	} else {
		// Nothing new to present: sleep ~1 vblank so the main loop doesn't
		// spin hot (eglSwapBuffers would otherwise provide the pacing).
		svcSleepThread(16666667ULL);
	}
}

void nx_webgl_exit(void) {
	if (!st)
		return;
	webgl_teardown_egl();
	for (int i = 0; i < K_COUNT; i++)
		st->protos[i].Reset();
}

void nx_init_webgl(v8::Isolate *iso, v8::Local<v8::Object> init_obj) {
	NX_SET_FUNC(init_obj, "webglContextNew", nx_webgl_context_new);
	NX_SET_FUNC(init_obj, "webglInitClass", nx_webgl_init_class);
}
