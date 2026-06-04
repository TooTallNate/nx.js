#include "dommatrix.h"
#include "error.h"
#include "wrap.h"
#include <math.h>
#include <stdlib.h>

using namespace v8;

#define RADS_PER_DEGREE (M_PI / 180.)

namespace {

// ---- pure math helpers (unchanged from the C version) ----
void matrix_values(nx_dommatrix_values_t *m, double *v) {
	m->m11 = v[0];  m->m12 = v[1];  m->m13 = v[2];  m->m14 = v[3];
	m->m21 = v[4];  m->m22 = v[5];  m->m23 = v[6];  m->m24 = v[7];
	m->m31 = v[8];  m->m32 = v[9];  m->m33 = v[10]; m->m34 = v[11];
	m->m41 = v[12]; m->m42 = v[13]; m->m43 = v[14]; m->m44 = v[15];
}

void multiply(const nx_dommatrix_values_t *a, const nx_dommatrix_values_t *b,
              nx_dommatrix_values_t *r) {
	*r = (nx_dommatrix_values_t){0};
	r->m11 = a->m11*b->m11 + a->m12*b->m21 + a->m13*b->m31 + a->m14*b->m41;
	r->m12 = a->m11*b->m12 + a->m12*b->m22 + a->m13*b->m32 + a->m14*b->m42;
	r->m13 = a->m11*b->m13 + a->m12*b->m23 + a->m13*b->m33 + a->m14*b->m43;
	r->m14 = a->m11*b->m14 + a->m12*b->m24 + a->m13*b->m34 + a->m14*b->m44;
	r->m21 = a->m21*b->m11 + a->m22*b->m21 + a->m23*b->m31 + a->m24*b->m41;
	r->m22 = a->m21*b->m12 + a->m22*b->m22 + a->m23*b->m32 + a->m24*b->m42;
	r->m23 = a->m21*b->m13 + a->m22*b->m23 + a->m23*b->m33 + a->m24*b->m43;
	r->m24 = a->m21*b->m14 + a->m22*b->m24 + a->m23*b->m34 + a->m24*b->m44;
	r->m31 = a->m31*b->m11 + a->m32*b->m21 + a->m33*b->m31 + a->m34*b->m41;
	r->m32 = a->m31*b->m12 + a->m32*b->m22 + a->m33*b->m32 + a->m34*b->m42;
	r->m33 = a->m31*b->m13 + a->m32*b->m23 + a->m33*b->m33 + a->m34*b->m43;
	r->m34 = a->m31*b->m14 + a->m32*b->m24 + a->m33*b->m34 + a->m34*b->m44;
	r->m41 = a->m41*b->m11 + a->m42*b->m21 + a->m43*b->m31 + a->m44*b->m41;
	r->m42 = a->m41*b->m12 + a->m42*b->m22 + a->m43*b->m32 + a->m44*b->m42;
	r->m43 = a->m41*b->m13 + a->m42*b->m23 + a->m43*b->m33 + a->m44*b->m43;
	r->m44 = a->m41*b->m14 + a->m42*b->m24 + a->m43*b->m34 + a->m44*b->m44;
}

void translate(nx_dommatrix_t *matrix, double tx, double ty, double tz) {
	if (tx != 0 || ty != 0 || tz != 0) {
		nx_dommatrix_values_t result, b;
		double v[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, tx, ty, tz, 1};
		matrix_values(&b, v);
		multiply(&b, &matrix->values, &result);
		matrix->values = result;
		if (tz != 0)
			matrix->is_2d = false;
	}
}

// ---- value extraction helpers ----
double num_at(Isolate *iso, Local<Object> arr, uint32_t idx, double def) {
	Local<Context> c = iso->GetCurrentContext();
	Local<Value> v;
	if (arr->Get(c, idx).ToLocal(&v) && v->IsNumber()) {
		double d;
		if (v->NumberValue(c).To(&d))
			return d;
	}
	return def;
}

double prop_num(Isolate *iso, Local<Object> obj, const char *key, double def) {
	Local<Context> c = iso->GetCurrentContext();
	Local<Value> v;
	if (obj->Get(c, nx_str(iso, key)).ToLocal(&v) && v->IsNumber()) {
		double d;
		if (v->NumberValue(c).To(&d))
			return d;
	}
	return def;
}

double arg_num(const FunctionCallbackInfo<Value> &info, int idx, double def) {
	if (info.Length() > idx && info[idx]->IsNumber()) {
		double d;
		if (info[idx]->NumberValue(info.GetIsolate()->GetCurrentContext())
		        .To(&d))
			return d;
	}
	return def;
}
bool arg_is_num(const FunctionCallbackInfo<Value> &info, int idx) {
	return info.Length() > idx && info[idx]->IsNumber();
}

nx_dommatrix_t *self_matrix(const FunctionCallbackInfo<Value> &info) {
	return nx::Unwrap<nx_dommatrix_t>(info.This());
}

void free_dommatrix(nx_dommatrix_t *m) { free(m); }

Local<Object> new_identity(Isolate *iso, nx_dommatrix_t **out) {
	nx_dommatrix_t *matrix =
	    (nx_dommatrix_t *)calloc(1, sizeof(nx_dommatrix_t));
	matrix->is_2d = true;
	matrix->values.m11 = matrix->values.m22 = matrix->values.m33 =
	    matrix->values.m44 = 1.;
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_dommatrix_t>(iso, obj, matrix, free_dommatrix);
	*out = matrix;
	return obj;
}

void nx_dommatrix_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_dommatrix_t *matrix;
	Local<Object> obj = new_identity(iso, &matrix);
	if (info.Length() > 0 && info[0]->IsArray()) {
		Local<Object> a = info[0].As<Object>();
		Local<Context> c = iso->GetCurrentContext();
		uint32_t length =
		    a->Get(c, nx_str(iso, "length")).ToLocalChecked()->Uint32Value(c)
		        .FromMaybe(0);
		if (length != 6 && length != 16) {
			nx_throw(iso, "Matrix init sequence must have a length of 6 or 16");
			return;
		}
		nx_dommatrix_values_t *m = &matrix->values;
		if (length == 6) {
			m->m11 = num_at(iso, a, 0, m->m11);
			m->m12 = num_at(iso, a, 1, m->m12);
			m->m21 = num_at(iso, a, 2, m->m21);
			m->m22 = num_at(iso, a, 3, m->m22);
			m->m41 = num_at(iso, a, 4, m->m41);
			m->m42 = num_at(iso, a, 5, m->m42);
		} else {
			double *f = &m->m11; // NOT contiguous; assign explicitly
			(void)f;
			m->m11 = num_at(iso, a, 0, m->m11);
			m->m12 = num_at(iso, a, 1, m->m12);
			m->m13 = num_at(iso, a, 2, m->m13);
			m->m14 = num_at(iso, a, 3, m->m14);
			m->m21 = num_at(iso, a, 4, m->m21);
			m->m22 = num_at(iso, a, 5, m->m22);
			m->m23 = num_at(iso, a, 6, m->m23);
			m->m24 = num_at(iso, a, 7, m->m24);
			m->m31 = num_at(iso, a, 8, m->m31);
			m->m32 = num_at(iso, a, 9, m->m32);
			m->m33 = num_at(iso, a, 10, m->m33);
			m->m34 = num_at(iso, a, 11, m->m34);
			m->m41 = num_at(iso, a, 12, m->m41);
			m->m42 = num_at(iso, a, 13, m->m42);
			m->m43 = num_at(iso, a, 14, m->m43);
			m->m44 = num_at(iso, a, 15, m->m44);
		}
	}
	info.GetReturnValue().Set(obj);
}

void nx_dommatrix_from_matrix(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_dommatrix_t *matrix;
	Local<Object> obj = new_identity(iso, &matrix);
	if (info.Length() > 0) {
		if (nx_dommatrix_init(iso, info[0], matrix))
			return;
	}
	info.GetReturnValue().Set(obj);
}

// ---- getters / setters via macros ----
#define GET(NAME)                                                              \
	void get_##NAME(const FunctionCallbackInfo<Value> &info) {                 \
		nx_dommatrix_t *m = self_matrix(info);                                 \
		if (m)                                                                 \
			info.GetReturnValue().Set(                                         \
			    Number::New(info.GetIsolate(), m->values.NAME));               \
	}
#define SET(NAME)                                                              \
	void set_##NAME(const FunctionCallbackInfo<Value> &info) {                 \
		nx_dommatrix_t *m = self_matrix(info);                                 \
		if (m)                                                                 \
			m->values.NAME = arg_num(info, 0, m->values.NAME);                 \
	}
#define SET3D(NAME, DEF)                                                       \
	void set_##NAME(const FunctionCallbackInfo<Value> &info) {                 \
		nx_dommatrix_t *m = self_matrix(info);                                 \
		if (!m)                                                                \
			return;                                                            \
		double val = arg_num(info, 0, m->values.NAME);                         \
		m->values.NAME = val;                                                  \
		if (val != DEF)                                                        \
			m->is_2d = false;                                                  \
	}

GET(m11) SET(m11) GET(m12) SET(m12)
GET(m13) SET3D(m13, 0.0) GET(m14) SET3D(m14, 0.0)
GET(m21) SET(m21) GET(m22) SET(m22)
GET(m23) SET3D(m23, 0.0) GET(m24) SET3D(m24, 0.0)
GET(m31) SET3D(m31, 0.0) GET(m32) SET3D(m32, 0.0)
GET(m33) SET3D(m33, 1.0) GET(m34) SET3D(m34, 0.0)
GET(m41) SET(m41) GET(m42) SET(m42)
GET(m43) SET3D(m43, 0.0) GET(m44) SET3D(m44, 1.0)

void get_is2D(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (m)
		info.GetReturnValue().Set(Boolean::New(info.GetIsolate(), m->is_2d));
}
void get_isIdentity(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (m)
		info.GetReturnValue().Set(
		    Boolean::New(info.GetIsolate(), nx_dommatrix_is_identity_(m)));
}

Local<Value> dup_this(const FunctionCallbackInfo<Value> &info) {
	return info.This();
}

void invert_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (m)
		nx_dommatrix_invert_self_(m);
	info.GetReturnValue().Set(dup_this(info));
}

// Resolve the "other" matrix argument, allocating+initializing if it's a
// plain DOMMatrixInit object rather than a wrapped DOMMatrix.
nx_dommatrix_t *resolve_other(Isolate *iso, Local<Value> arg, bool *needs_free) {
	nx_dommatrix_t *other = nx::Unwrap<nx_dommatrix_t>(arg);
	*needs_free = false;
	if (!other) {
		other = (nx_dommatrix_t *)calloc(1, sizeof(nx_dommatrix_t));
		*needs_free = true;
		if (nx_dommatrix_init(iso, arg, other)) {
			free(other);
			return nullptr;
		}
	}
	return other;
}

void multiply_self(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_dommatrix_t *m = self_matrix(info);
	if (m && info.Length() == 1 && info[0]->IsObject()) {
		bool needs_free;
		nx_dommatrix_t *other = resolve_other(iso, info[0], &needs_free);
		if (other) {
			nx_dommatrix_values_t result;
			multiply(&other->values, &m->values, &result);
			m->values = result;
			if (needs_free)
				free(other);
		}
	}
	info.GetReturnValue().Set(dup_this(info));
}

void premultiply_self(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_dommatrix_t *m = self_matrix(info);
	if (m && info.Length() == 1 && info[0]->IsObject()) {
		bool needs_free;
		nx_dommatrix_t *other = resolve_other(iso, info[0], &needs_free);
		if (other) {
			nx_dommatrix_values_t result;
			multiply(&m->values, &other->values, &result);
			m->values = result;
			if (needs_free)
				free(other);
		}
	}
	info.GetReturnValue().Set(dup_this(info));
}

void rotate_axis_angle_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (!m) {
		info.GetReturnValue().Set(dup_this(info));
		return;
	}
	double x = arg_num(info, 0, 0), y = arg_num(info, 1, 0),
	       z = arg_num(info, 2, 0), angle = arg_num(info, 3, 0);
	double length = sqrt(x * x + y * y + z * z);
	if (length == 0.) {
		info.GetReturnValue().Set(dup_this(info));
		return;
	}
	if (length != 1.) {
		x /= length;
		y /= length;
		z /= length;
	}
	angle *= RADS_PER_DEGREE;
	double c = cos(angle), s = sin(angle), t = 1. - c, tx = t * x, ty = t * y;
	nx_dommatrix_values_t b, result;
	double v[] = {tx*x + c, tx*y + s*z, tx*z - s*y, 0,
	              tx*y - s*z, ty*y + c, ty*z + s*x, 0,
	              tx*z + s*y, ty*z - s*x, t*z*z + c, 0, 0, 0, 0, 1};
	matrix_values(&b, v);
	multiply(&b, &m->values, &result);
	m->values = result;
	if (x != 0 || y != 0)
		m->is_2d = false;
	info.GetReturnValue().Set(dup_this(info));
}

void rotate_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (!m) {
		info.GetReturnValue().Set(dup_this(info));
		return;
	}
	double rotX = arg_num(info, 0, 0), rotY = 0, rotZ = 0;
	if (!arg_is_num(info, 1) && !arg_is_num(info, 2)) {
		rotZ = rotX;
		rotX = 0;
	} else {
		rotY = arg_num(info, 1, 0);
		rotZ = arg_num(info, 2, 0);
	}
	if (rotX != 0 || rotY != 0)
		m->is_2d = false;
	rotX *= RADS_PER_DEGREE;
	rotY *= RADS_PER_DEGREE;
	rotZ *= RADS_PER_DEGREE;
	double c, s;
	nx_dommatrix_values_t result, b;
	c = cos(rotZ);
	s = sin(rotZ);
	double vz[] = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	matrix_values(&b, vz);
	multiply(&b, &m->values, &result);
	m->values = result;
	c = cos(rotY);
	s = sin(rotY);
	double vy[] = {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1};
	matrix_values(&b, vy);
	multiply(&b, &m->values, &result);
	m->values = result;
	c = cos(rotX);
	s = sin(rotX);
	double vx[] = {1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1};
	matrix_values(&b, vx);
	multiply(&b, &m->values, &result);
	m->values = result;
	info.GetReturnValue().Set(dup_this(info));
}

void scale_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (!m) {
		info.GetReturnValue().Set(dup_this(info));
		return;
	}
	double originX = arg_num(info, 3, 0), originY = arg_num(info, 4, 0),
	       originZ = arg_num(info, 5, 0);
	translate(m, originX, originY, originZ);
	double scaleX = arg_num(info, 0, 1);
	double scaleY = arg_num(info, 1, scaleX);
	double scaleZ = arg_num(info, 2, 1);
	nx_dommatrix_values_t result, b;
	double v[] = {scaleX, 0, 0, 0, 0, scaleY, 0, 0,
	              0, 0, scaleZ, 0, 0, 0, 0, 1};
	matrix_values(&b, v);
	multiply(&b, &m->values, &result);
	m->values = result;
	translate(m, -originX, -originY, -originZ);
	if (scaleZ != 1 || originZ != 0)
		m->is_2d = false;
	info.GetReturnValue().Set(dup_this(info));
}

void skew_x_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (m) {
		double sx = arg_num(info, 0, 0);
		if (sx != 0) {
			double t = tan(sx * RADS_PER_DEGREE);
			nx_dommatrix_values_t result, b;
			double v[] = {1, 0, 0, 0, t, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
			matrix_values(&b, v);
			multiply(&b, &m->values, &result);
			m->values = result;
		}
	}
	info.GetReturnValue().Set(dup_this(info));
}

void skew_y_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (m) {
		double sy = arg_num(info, 0, 0);
		if (sy != 0) {
			double t = tan(sy * RADS_PER_DEGREE);
			nx_dommatrix_values_t result, b;
			double v[] = {1, t, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
			matrix_values(&b, v);
			multiply(&b, &m->values, &result);
			m->values = result;
		}
	}
	info.GetReturnValue().Set(dup_this(info));
}

void translate_self(const FunctionCallbackInfo<Value> &info) {
	nx_dommatrix_t *m = self_matrix(info);
	if (m)
		translate(m, arg_num(info, 0, 0), arg_num(info, 1, 0),
		          arg_num(info, 2, 0));
	info.GetReturnValue().Set(dup_this(info));
}

void transform_point(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_dommatrix_t *matrix = nx::Unwrap<nx_dommatrix_t>(info[0]);
	if (!matrix)
		return;
	Local<Object> p = info[1].As<Object>();
	double x = prop_num(iso, p, "x", 0), y = prop_num(iso, p, "y", 0),
	       z = prop_num(iso, p, "z", 0), w = prop_num(iso, p, "w", 1);
	nx_dommatrix_transform_point_(matrix, &x, &y, &z, &w);
	Local<Object> point = Object::New(iso);
	point->Set(context, nx_str(iso, "x"), Number::New(iso, x)).Check();
	point->Set(context, nx_str(iso, "y"), Number::New(iso, y)).Check();
	point->Set(context, nx_str(iso, "z"), Number::New(iso, z)).Check();
	point->Set(context, nx_str(iso, "w"), Number::New(iso, w)).Check();
	info.GetReturnValue().Set(point);
}

Local<Object> proto_of(Isolate *iso, const FunctionCallbackInfo<Value> &info) {
	Local<Context> context = iso->GetCurrentContext();
	return info[0]
	    .As<Object>()
	    ->Get(context, nx_str(iso, "prototype"))
	    .ToLocalChecked()
	    .As<Object>();
}

void ro_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = proto_of(iso, info);
	NX_DEF_GET(proto, "a", get_m11);
	NX_DEF_GET(proto, "b", get_m12);
	NX_DEF_GET(proto, "c", get_m21);
	NX_DEF_GET(proto, "d", get_m22);
	NX_DEF_GET(proto, "e", get_m41);
	NX_DEF_GET(proto, "f", get_m42);
#define RO(N) NX_DEF_GET(proto, #N, get_##N)
	RO(m11); RO(m12); RO(m13); RO(m14);
	RO(m21); RO(m22); RO(m23); RO(m24);
	RO(m31); RO(m32); RO(m33); RO(m34);
	RO(m41); RO(m42); RO(m43); RO(m44);
#undef RO
	NX_DEF_GET(proto, "is2D", get_is2D);
	NX_DEF_GET(proto, "isIdentity", get_isIdentity);
}

void init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = proto_of(iso, info);
	NX_DEF_GETSET(proto, "a", get_m11, set_m11);
	NX_DEF_GETSET(proto, "b", get_m12, set_m12);
	NX_DEF_GETSET(proto, "c", get_m21, set_m21);
	NX_DEF_GETSET(proto, "d", get_m22, set_m22);
	NX_DEF_GETSET(proto, "e", get_m41, set_m41);
	NX_DEF_GETSET(proto, "f", get_m42, set_m42);
#define RW(N) NX_DEF_GETSET(proto, #N, get_##N, set_##N)
	RW(m11); RW(m12); RW(m13); RW(m14);
	RW(m21); RW(m22); RW(m23); RW(m24);
	RW(m31); RW(m32); RW(m33); RW(m34);
	RW(m41); RW(m42); RW(m43); RW(m44);
#undef RW
	NX_DEF_FUNC(proto, "invertSelf", invert_self, 0);
	NX_DEF_FUNC(proto, "multiplySelf", multiply_self, 0);
	NX_DEF_FUNC(proto, "preMultiplySelf", premultiply_self, 0);
	NX_DEF_FUNC(proto, "rotateAxisAngleSelf", rotate_axis_angle_self, 0);
	NX_DEF_FUNC(proto, "rotateSelf", rotate_self, 0);
	NX_DEF_FUNC(proto, "scaleSelf", scale_self, 0);
	NX_DEF_FUNC(proto, "skewXSelf", skew_x_self, 0);
	NX_DEF_FUNC(proto, "skewYSelf", skew_y_self, 0);
	NX_DEF_FUNC(proto, "translateSelf", translate_self, 0);
}

} // namespace

// ---- shared (non-namespaced) helpers used by canvas ----

nx_dommatrix_t *nx_get_dommatrix(Isolate *iso, Local<Value> obj) {
	(void)iso;
	return nx::Unwrap<nx_dommatrix_t>(obj);
}

int nx_dommatrix_init(Isolate *iso, Local<Value> obj_val,
                      nx_dommatrix_t *matrix) {
	if (obj_val.IsEmpty() || !obj_val->IsObject())
		return 0;
	Local<Object> obj = obj_val.As<Object>();
	nx_dommatrix_values_t *m = &matrix->values;
	m->m11 = prop_num(iso, obj, "a", m->m11);
	m->m12 = prop_num(iso, obj, "b", m->m12);
	m->m21 = prop_num(iso, obj, "c", m->m21);
	m->m22 = prop_num(iso, obj, "d", m->m22);
	m->m41 = prop_num(iso, obj, "e", m->m41);
	m->m42 = prop_num(iso, obj, "f", m->m42);
	m->m11 = prop_num(iso, obj, "m11", m->m11);
	m->m12 = prop_num(iso, obj, "m12", m->m12);
	m->m13 = prop_num(iso, obj, "m13", m->m13);
	m->m14 = prop_num(iso, obj, "m14", m->m14);
	m->m21 = prop_num(iso, obj, "m21", m->m21);
	m->m22 = prop_num(iso, obj, "m22", m->m22);
	m->m23 = prop_num(iso, obj, "m23", m->m23);
	m->m24 = prop_num(iso, obj, "m24", m->m24);
	m->m31 = prop_num(iso, obj, "m31", m->m31);
	m->m32 = prop_num(iso, obj, "m32", m->m32);
	m->m33 = prop_num(iso, obj, "m33", m->m33);
	m->m34 = prop_num(iso, obj, "m34", m->m34);
	m->m41 = prop_num(iso, obj, "m41", m->m41);
	m->m42 = prop_num(iso, obj, "m42", m->m42);
	m->m43 = prop_num(iso, obj, "m43", m->m43);
	m->m44 = prop_num(iso, obj, "m44", m->m44);
	Local<Context> c = iso->GetCurrentContext();
	Local<Value> is2d;
	if (obj->Get(c, nx_str(iso, "is2D")).ToLocal(&is2d) && is2d->IsBoolean()) {
		matrix->is_2d = is2d->BooleanValue(iso);
	}
	return 0;
}

bool nx_dommatrix_is_identity_(nx_dommatrix_t *matrix) {
	nx_dommatrix_values_t *v = &matrix->values;
	return v->m11 == 1. && v->m12 == 0. && v->m13 == 0. && v->m14 == 0. &&
	       v->m21 == 0. && v->m22 == 1. && v->m23 == 0. && v->m24 == 0. &&
	       v->m31 == 0. && v->m32 == 0. && v->m33 == 1. && v->m34 == 0. &&
	       v->m41 == 0. && v->m42 == 0. && v->m43 == 0. && v->m44 == 1.;
}

void nx_dommatrix_transform_point_(nx_dommatrix_t *m, double *x, double *y,
                                   double *z, double *w) {
	double nx = m->values.m11*(*x) + m->values.m21*(*y) + m->values.m31*(*z) +
	            m->values.m41*(*w);
	double ny = m->values.m12*(*x) + m->values.m22*(*y) + m->values.m32*(*z) +
	            m->values.m42*(*w);
	double nz = m->values.m13*(*x) + m->values.m23*(*y) + m->values.m33*(*z) +
	            m->values.m43*(*w);
	double nw = m->values.m14*(*x) + m->values.m24*(*y) + m->values.m34*(*z) +
	            m->values.m44*(*w);
	*x = nx;
	*y = ny;
	*z = nz;
	*w = nw;
}

void nx_dommatrix_invert_self_(nx_dommatrix_t *matrix) {
	double inv[16] = {0};
	nx_dommatrix_values_t *V = &matrix->values;
	inv[0] = V->m22*V->m33*V->m44 - V->m22*V->m34*V->m43 - V->m32*V->m23*V->m44 +
	         V->m32*V->m24*V->m43 + V->m42*V->m23*V->m34 - V->m42*V->m24*V->m33;
	inv[4] = -V->m21*V->m33*V->m44 + V->m21*V->m34*V->m43 + V->m31*V->m23*V->m44 -
	         V->m31*V->m24*V->m43 - V->m41*V->m23*V->m34 + V->m41*V->m24*V->m33;
	inv[8] = V->m21*V->m32*V->m44 - V->m21*V->m34*V->m42 - V->m31*V->m22*V->m44 +
	         V->m31*V->m24*V->m42 + V->m41*V->m22*V->m34 - V->m41*V->m24*V->m32;
	inv[12] = -V->m21*V->m32*V->m43 + V->m21*V->m33*V->m42 + V->m31*V->m22*V->m43 -
	          V->m31*V->m23*V->m42 - V->m41*V->m22*V->m33 + V->m41*V->m23*V->m32;
	double det = V->m11*inv[0] + V->m12*inv[4] + V->m13*inv[8] + V->m14*inv[12];
	if (det == 0) {
		V->m11 = V->m12 = V->m13 = V->m14 = V->m21 = V->m22 = V->m23 = V->m24 =
		    V->m31 = V->m32 = V->m33 = V->m34 = V->m41 = V->m42 = V->m43 =
		        V->m44 = NAN;
		matrix->is_2d = false;
		return;
	}
	inv[1] = -V->m12*V->m33*V->m44 + V->m12*V->m34*V->m43 + V->m32*V->m13*V->m44 -
	         V->m32*V->m14*V->m43 - V->m42*V->m13*V->m34 + V->m42*V->m14*V->m33;
	inv[5] = V->m11*V->m33*V->m44 - V->m11*V->m34*V->m43 - V->m31*V->m13*V->m44 +
	         V->m31*V->m14*V->m43 + V->m41*V->m13*V->m34 - V->m41*V->m14*V->m33;
	inv[9] = -V->m11*V->m32*V->m44 + V->m11*V->m34*V->m42 + V->m31*V->m12*V->m44 -
	         V->m31*V->m14*V->m42 - V->m41*V->m12*V->m34 + V->m41*V->m14*V->m32;
	inv[13] = V->m11*V->m32*V->m43 - V->m11*V->m33*V->m42 - V->m31*V->m12*V->m43 +
	          V->m31*V->m13*V->m42 + V->m41*V->m12*V->m33 - V->m41*V->m13*V->m32;
	inv[2] = V->m12*V->m23*V->m44 - V->m12*V->m24*V->m43 - V->m22*V->m13*V->m44 +
	         V->m22*V->m14*V->m43 + V->m42*V->m13*V->m24 - V->m42*V->m14*V->m23;
	inv[6] = -V->m11*V->m23*V->m44 + V->m11*V->m24*V->m43 + V->m21*V->m13*V->m44 -
	         V->m21*V->m14*V->m43 - V->m41*V->m13*V->m24 + V->m41*V->m14*V->m23;
	inv[10] = V->m11*V->m22*V->m44 - V->m11*V->m24*V->m42 - V->m21*V->m12*V->m44 +
	          V->m21*V->m14*V->m42 + V->m41*V->m12*V->m24 - V->m41*V->m14*V->m22;
	inv[14] = -V->m11*V->m22*V->m43 + V->m11*V->m23*V->m42 + V->m21*V->m12*V->m43 -
	          V->m21*V->m13*V->m42 - V->m41*V->m12*V->m23 + V->m41*V->m13*V->m22;
	inv[3] = -V->m12*V->m23*V->m34 + V->m12*V->m24*V->m33 + V->m22*V->m13*V->m34 -
	         V->m22*V->m14*V->m33 - V->m32*V->m13*V->m24 + V->m32*V->m14*V->m23;
	inv[7] = V->m11*V->m23*V->m34 - V->m11*V->m24*V->m33 - V->m21*V->m13*V->m34 +
	         V->m21*V->m14*V->m33 + V->m31*V->m13*V->m24 - V->m31*V->m14*V->m23;
	inv[11] = -V->m11*V->m22*V->m34 + V->m11*V->m24*V->m32 + V->m21*V->m12*V->m34 -
	          V->m21*V->m14*V->m32 - V->m31*V->m12*V->m24 + V->m31*V->m14*V->m22;
	inv[15] = V->m11*V->m22*V->m33 - V->m11*V->m23*V->m32 - V->m21*V->m12*V->m33 +
	          V->m21*V->m13*V->m32 + V->m31*V->m12*V->m23 - V->m31*V->m13*V->m22;
	double *out = &V->m11;
	(void)out;
	V->m11 = inv[0]/det;  V->m12 = inv[1]/det;  V->m13 = inv[2]/det;
	V->m14 = inv[3]/det;  V->m21 = inv[4]/det;  V->m22 = inv[5]/det;
	V->m23 = inv[6]/det;  V->m24 = inv[7]/det;  V->m31 = inv[8]/det;
	V->m32 = inv[9]/det;  V->m33 = inv[10]/det; V->m34 = inv[11]/det;
	V->m41 = inv[12]/det; V->m42 = inv[13]/det; V->m43 = inv[14]/det;
	V->m44 = inv[15]/det;
}

void nx_init_dommatrix(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "dommatrixNew", nx_dommatrix_new);
	NX_SET_FUNC(init_obj, "dommatrixFromMatrix", nx_dommatrix_from_matrix);
	NX_SET_FUNC(init_obj, "dommatrixROInitClass", ro_init_class);
	NX_SET_FUNC(init_obj, "dommatrixInitClass", init_class);
	NX_SET_FUNC(init_obj, "dommatrixTransformPoint", transform_point);
}
