#include "dommatrix.h"
#include <math.h>

#define RADS_PER_DEGREE (M_PI / 180.)

#define ARG_IS_NUM(INDEX) (argc > INDEX && JS_IsNumber(argv[INDEX]))

#define ARG_TO_NUM(INDEX, VAR)                                                 \
	if (ARG_IS_NUM(INDEX)) {                                                   \
		if (JS_ToFloat64(ctx, &VAR, argv[INDEX])) {                            \
			return JS_EXCEPTION;                                               \
		}                                                                      \
	}

#define DOMMATRIX_THIS                                                         \
	nx_dommatrix_t *matrix =                                                   \
		JS_GetOpaque2(ctx, this_val, nx_dommatrix_class_id);                   \
	if (!matrix) {                                                             \
		return JS_EXCEPTION;                                                   \
	}

static JSClassID nx_dommatrix_class_id;

nx_dommatrix_t *nx_get_dommatrix(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_dommatrix_class_id);
}

void matrix_values(nx_dommatrix_values_t *m, double *v) {
	m->m11 = v[0];
	m->m12 = v[1];
	m->m13 = v[2];
	m->m14 = v[3];
	m->m21 = v[4];
	m->m22 = v[5];
	m->m23 = v[6];
	m->m24 = v[7];
	m->m31 = v[8];
	m->m32 = v[9];
	m->m33 = v[10];
	m->m34 = v[11];
	m->m41 = v[12];
	m->m42 = v[13];
	m->m43 = v[14];
	m->m44 = v[15];
}

void multiply(const nx_dommatrix_values_t *a, const nx_dommatrix_values_t *b,
			  nx_dommatrix_values_t *result) {
	*result = (nx_dommatrix_values_t){0};

	result->m11 =
		a->m11 * b->m11 + a->m12 * b->m21 + a->m13 * b->m31 + a->m14 * b->m41;
	result->m12 =
		a->m11 * b->m12 + a->m12 * b->m22 + a->m13 * b->m32 + a->m14 * b->m42;
	result->m13 =
		a->m11 * b->m13 + a->m12 * b->m23 + a->m13 * b->m33 + a->m14 * b->m43;
	result->m14 =
		a->m11 * b->m14 + a->m12 * b->m24 + a->m13 * b->m34 + a->m14 * b->m44;

	result->m21 =
		a->m21 * b->m11 + a->m22 * b->m21 + a->m23 * b->m31 + a->m24 * b->m41;
	result->m22 =
		a->m21 * b->m12 + a->m22 * b->m22 + a->m23 * b->m32 + a->m24 * b->m42;
	result->m23 =
		a->m21 * b->m13 + a->m22 * b->m23 + a->m23 * b->m33 + a->m24 * b->m43;
	result->m24 =
		a->m21 * b->m14 + a->m22 * b->m24 + a->m23 * b->m34 + a->m24 * b->m44;

	result->m31 =
		a->m31 * b->m11 + a->m32 * b->m21 + a->m33 * b->m31 + a->m34 * b->m41;
	result->m32 =
		a->m31 * b->m12 + a->m32 * b->m22 + a->m33 * b->m32 + a->m34 * b->m42;
	result->m33 =
		a->m31 * b->m13 + a->m32 * b->m23 + a->m33 * b->m33 + a->m34 * b->m43;
	result->m34 =
		a->m31 * b->m14 + a->m32 * b->m24 + a->m33 * b->m34 + a->m34 * b->m44;

	result->m41 =
		a->m41 * b->m11 + a->m42 * b->m21 + a->m43 * b->m31 + a->m44 * b->m41;
	result->m42 =
		a->m41 * b->m12 + a->m42 * b->m22 + a->m43 * b->m32 + a->m44 * b->m42;
	result->m43 =
		a->m41 * b->m13 + a->m42 * b->m23 + a->m43 * b->m33 + a->m44 * b->m43;
	result->m44 =
		a->m41 * b->m14 + a->m42 * b->m24 + a->m43 * b->m34 + a->m44 * b->m44;
}

#define NX_DOMMATRIX_NEW_GET_INDEX(INDEX, FIELD)                               \
	v = JS_GetPropertyUint32(ctx, argv[0], INDEX);                             \
	if (JS_IsNumber(v)) {                                                      \
		if (JS_ToFloat64(ctx, &matrix->values.FIELD, v)) {                     \
			return JS_EXCEPTION;                                               \
		}                                                                      \
	}                                                                          \
	JS_FreeValue(ctx, v);

static JSValue nx_dommatrix_new(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	nx_dommatrix_t *matrix = js_mallocz(ctx, sizeof(nx_dommatrix_t));
	if (!matrix) {
		return JS_EXCEPTION;
	}

	// Initialize as identity matrix
	matrix->is_2d = true;
	matrix->values.m11 = matrix->values.m22 = matrix->values.m33 =
		matrix->values.m44 = 1.;

	if (argc > 0 && JS_IsArray(ctx, argv[0])) {
		int length;
		JSValue lengthVal = JS_GetPropertyStr(ctx, argv[0], "length");
		if (JS_ToInt32(ctx, &length, lengthVal)) {
			js_free(ctx, matrix);
			return JS_EXCEPTION;
		}
		JS_FreeValue(ctx, lengthVal);
		if (length != 6 && length != 16) {
			js_free(ctx, matrix);
			return JS_ThrowTypeError(ctx,
									 "Matrix init sequence must have a length "
									 "of 6 or 16 (actual value: %d)",
									 length);
		}
		JSValue v;
		if (length == 6) {
			NX_DOMMATRIX_NEW_GET_INDEX(0, m11)
			NX_DOMMATRIX_NEW_GET_INDEX(1, m12)
			NX_DOMMATRIX_NEW_GET_INDEX(2, m21)
			NX_DOMMATRIX_NEW_GET_INDEX(3, m22)
			NX_DOMMATRIX_NEW_GET_INDEX(4, m41)
			NX_DOMMATRIX_NEW_GET_INDEX(5, m42)
		} else {
			NX_DOMMATRIX_NEW_GET_INDEX(0, m11)
			NX_DOMMATRIX_NEW_GET_INDEX(1, m12)
			NX_DOMMATRIX_NEW_GET_INDEX(2, m13)
			NX_DOMMATRIX_NEW_GET_INDEX(3, m14)
			NX_DOMMATRIX_NEW_GET_INDEX(4, m21)
			NX_DOMMATRIX_NEW_GET_INDEX(5, m22)
			NX_DOMMATRIX_NEW_GET_INDEX(6, m23)
			NX_DOMMATRIX_NEW_GET_INDEX(7, m24)
			NX_DOMMATRIX_NEW_GET_INDEX(8, m31)
			NX_DOMMATRIX_NEW_GET_INDEX(9, m32)
			NX_DOMMATRIX_NEW_GET_INDEX(10, m33)
			NX_DOMMATRIX_NEW_GET_INDEX(11, m34)
			NX_DOMMATRIX_NEW_GET_INDEX(12, m41)
			NX_DOMMATRIX_NEW_GET_INDEX(13, m42)
			NX_DOMMATRIX_NEW_GET_INDEX(14, m43)
			NX_DOMMATRIX_NEW_GET_INDEX(15, m44)
		}
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_dommatrix_class_id);
	JS_SetOpaque(obj, matrix);
	return obj;
}

#define NX_DOMMATRIX_INIT_GET_PROP(NAME, FIELD)                                \
	v = JS_GetPropertyStr(ctx, obj, #NAME);                                    \
	if (JS_IsNumber(v)) {                                                      \
		if (JS_ToFloat64(ctx, &matrix->values.FIELD, v)) {                     \
			return 1;                                                          \
		}                                                                      \
	}                                                                          \
	JS_FreeValue(ctx, v);

int nx_dommatrix_init(JSContext *ctx, JSValueConst obj,
					  nx_dommatrix_t *matrix) {
	if (!JS_IsObject(obj))
		return 0;

	JSValue v;
	NX_DOMMATRIX_INIT_GET_PROP(a, m11)
	NX_DOMMATRIX_INIT_GET_PROP(b, m12)
	NX_DOMMATRIX_INIT_GET_PROP(c, m21)
	NX_DOMMATRIX_INIT_GET_PROP(d, m22)
	NX_DOMMATRIX_INIT_GET_PROP(e, m41)
	NX_DOMMATRIX_INIT_GET_PROP(f, m42)
	NX_DOMMATRIX_INIT_GET_PROP(m11, m11)
	NX_DOMMATRIX_INIT_GET_PROP(m12, m12)
	NX_DOMMATRIX_INIT_GET_PROP(m13, m13)
	NX_DOMMATRIX_INIT_GET_PROP(m14, m14)
	NX_DOMMATRIX_INIT_GET_PROP(m21, m21)
	NX_DOMMATRIX_INIT_GET_PROP(m22, m22)
	NX_DOMMATRIX_INIT_GET_PROP(m23, m23)
	NX_DOMMATRIX_INIT_GET_PROP(m24, m24)
	NX_DOMMATRIX_INIT_GET_PROP(m31, m31)
	NX_DOMMATRIX_INIT_GET_PROP(m32, m32)
	NX_DOMMATRIX_INIT_GET_PROP(m33, m33)
	NX_DOMMATRIX_INIT_GET_PROP(m34, m34)
	NX_DOMMATRIX_INIT_GET_PROP(m41, m41)
	NX_DOMMATRIX_INIT_GET_PROP(m42, m42)
	NX_DOMMATRIX_INIT_GET_PROP(m43, m43)
	NX_DOMMATRIX_INIT_GET_PROP(m44, m44)

	v = JS_GetPropertyStr(ctx, obj, "is2D");
	if (JS_IsBool(v)) {
		int b = JS_ToBool(ctx, v);
		if (b == -1) {
			return 1;
		}
		matrix->is_2d = b;
	}
	JS_FreeValue(ctx, v);

	return 0;
}

static JSValue nx_dommatrix_from_matrix(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_dommatrix_t *matrix = js_mallocz(ctx, sizeof(nx_dommatrix_t));
	if (!matrix) {
		return JS_EXCEPTION;
	}

	// Initialize as identity matrix
	matrix->is_2d = true;
	matrix->values.m11 = matrix->values.m22 = matrix->values.m33 =
		matrix->values.m44 = 1.;

	if (argc > 0) {
		if (nx_dommatrix_init(ctx, argv[0], matrix)) {
			return JS_EXCEPTION;
		}
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_dommatrix_class_id);
	JS_SetOpaque(obj, matrix);
	return obj;
}

#define DEFINE_GETTER(NAME)                                                    \
	static JSValue nx_dommatrix_get_##NAME(                                    \
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { \
		DOMMATRIX_THIS;                                                        \
		return JS_NewFloat64(ctx, matrix->values.NAME);                        \
	}

#define DEFINE_GETTER_SETTER(NAME)                                             \
	DEFINE_GETTER(NAME)                                                        \
	static JSValue nx_dommatrix_set_##NAME(                                    \
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { \
		DOMMATRIX_THIS;                                                        \
		if (JS_ToFloat64(ctx, &matrix->values.NAME, argv[0])) {                \
			return JS_EXCEPTION;                                               \
		}                                                                      \
		return JS_UNDEFINED;                                                   \
	}

#define DEFINE_GETTER_SETTER_3D(NAME, DEFAULT_VAL)                             \
	DEFINE_GETTER(NAME)                                                        \
	static JSValue nx_dommatrix_set_##NAME(                                    \
		JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) { \
		DOMMATRIX_THIS;                                                        \
		double val;                                                            \
		if (JS_ToFloat64(ctx, &val, argv[0])) {                                \
			return JS_EXCEPTION;                                               \
		}                                                                      \
		matrix->values.NAME = val;                                             \
		if (val != DEFAULT_VAL) {                                              \
			matrix->is_2d = false;                                             \
		}                                                                      \
		return JS_UNDEFINED;                                                   \
	}

DEFINE_GETTER_SETTER(m11)
DEFINE_GETTER_SETTER(m12)
DEFINE_GETTER_SETTER_3D(m13, 0.0)
DEFINE_GETTER_SETTER_3D(m14, 0.0)
DEFINE_GETTER_SETTER(m21)
DEFINE_GETTER_SETTER(m22)
DEFINE_GETTER_SETTER_3D(m23, 0.0)
DEFINE_GETTER_SETTER_3D(m24, 0.0)
DEFINE_GETTER_SETTER_3D(m31, 0.0)
DEFINE_GETTER_SETTER_3D(m32, 0.0)
DEFINE_GETTER_SETTER_3D(m33, 1.0)
DEFINE_GETTER_SETTER_3D(m34, 0.0)
DEFINE_GETTER_SETTER(m41)
DEFINE_GETTER_SETTER(m42)
DEFINE_GETTER_SETTER_3D(m43, 0.0)
DEFINE_GETTER_SETTER_3D(m44, 1.0)

static JSValue nx_dommatrix_is_2d(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	return JS_NewBool(ctx, matrix->is_2d);
}

bool nx_dommatrix_is_identity_(nx_dommatrix_t *matrix) {
	nx_dommatrix_values_t *values = &matrix->values;
	return values->m11 == 1. && values->m12 == 0. && values->m13 == 0. &&
		   values->m14 == 0. && values->m21 == 0. && values->m22 == 1. &&
		   values->m23 == 0. && values->m24 == 0. && values->m31 == 0. &&
		   values->m32 == 0. && values->m33 == 1. && values->m34 == 0. &&
		   values->m41 == 0. && values->m42 == 0. && values->m43 == 0. &&
		   values->m44 == 1.;
}

static JSValue nx_dommatrix_is_identity(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	return JS_NewBool(ctx, nx_dommatrix_is_identity_(matrix));
}

void translate(nx_dommatrix_t *matrix, double tx, double ty, double tz) {
	if (tx != 0 || ty != 0 || tz != 0) {
		nx_dommatrix_values_t result;
		nx_dommatrix_values_t b;
		double values[] = {
			1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, tx, ty, tz, 1,
		};
		matrix_values(&b, values);
		multiply(&b, &matrix->values, &result);
		*(&matrix->values) = result;
		if (tz != 0) {
			matrix->is_2d = false;
		}
	}
}

void nx_dommatrix_invert_self_(nx_dommatrix_t *matrix) {
	double inv[16] = {0};

	inv[0] = matrix->values.m22 * matrix->values.m33 * matrix->values.m44 -
			 matrix->values.m22 * matrix->values.m34 * matrix->values.m43 -
			 matrix->values.m32 * matrix->values.m23 * matrix->values.m44 +
			 matrix->values.m32 * matrix->values.m24 * matrix->values.m43 +
			 matrix->values.m42 * matrix->values.m23 * matrix->values.m34 -
			 matrix->values.m42 * matrix->values.m24 * matrix->values.m33;

	inv[4] = -matrix->values.m21 * matrix->values.m33 * matrix->values.m44 +
			 matrix->values.m21 * matrix->values.m34 * matrix->values.m43 +
			 matrix->values.m31 * matrix->values.m23 * matrix->values.m44 -
			 matrix->values.m31 * matrix->values.m24 * matrix->values.m43 -
			 matrix->values.m41 * matrix->values.m23 * matrix->values.m34 +
			 matrix->values.m41 * matrix->values.m24 * matrix->values.m33;

	inv[8] = matrix->values.m21 * matrix->values.m32 * matrix->values.m44 -
			 matrix->values.m21 * matrix->values.m34 * matrix->values.m42 -
			 matrix->values.m31 * matrix->values.m22 * matrix->values.m44 +
			 matrix->values.m31 * matrix->values.m24 * matrix->values.m42 +
			 matrix->values.m41 * matrix->values.m22 * matrix->values.m34 -
			 matrix->values.m41 * matrix->values.m24 * matrix->values.m32;

	inv[12] = -matrix->values.m21 * matrix->values.m32 * matrix->values.m43 +
			  matrix->values.m21 * matrix->values.m33 * matrix->values.m42 +
			  matrix->values.m31 * matrix->values.m22 * matrix->values.m43 -
			  matrix->values.m31 * matrix->values.m23 * matrix->values.m42 -
			  matrix->values.m41 * matrix->values.m22 * matrix->values.m33 +
			  matrix->values.m41 * matrix->values.m23 * matrix->values.m32;

	double det = matrix->values.m11 * inv[0] + matrix->values.m12 * inv[4] +
				 matrix->values.m13 * inv[8] + matrix->values.m14 * inv[12];

	// If the determinant is zero, this matrix cannot be inverted, and all
	// values should be set to `NaN`, with the `is2D` flag set to `false`.
	if (det == 0) {
		matrix->values.m11 = matrix->values.m12 = matrix->values.m13 =
			matrix->values.m14 = matrix->values.m21 = matrix->values.m22 =
				matrix->values.m23 = matrix->values.m24 = matrix->values.m31 =
					matrix->values.m32 = matrix->values.m33 =
						matrix->values.m34 = matrix->values.m41 =
							matrix->values.m42 = matrix->values.m43 =
								matrix->values.m44 = NAN;
		matrix->is_2d = false;
	} else {
		inv[1] = -matrix->values.m12 * matrix->values.m33 * matrix->values.m44 +
				 matrix->values.m12 * matrix->values.m34 * matrix->values.m43 +
				 matrix->values.m32 * matrix->values.m13 * matrix->values.m44 -
				 matrix->values.m32 * matrix->values.m14 * matrix->values.m43 -
				 matrix->values.m42 * matrix->values.m13 * matrix->values.m34 +
				 matrix->values.m42 * matrix->values.m14 * matrix->values.m33;

		inv[5] = matrix->values.m11 * matrix->values.m33 * matrix->values.m44 -
				 matrix->values.m11 * matrix->values.m34 * matrix->values.m43 -
				 matrix->values.m31 * matrix->values.m13 * matrix->values.m44 +
				 matrix->values.m31 * matrix->values.m14 * matrix->values.m43 +
				 matrix->values.m41 * matrix->values.m13 * matrix->values.m34 -
				 matrix->values.m41 * matrix->values.m14 * matrix->values.m33;

		inv[9] = -matrix->values.m11 * matrix->values.m32 * matrix->values.m44 +
				 matrix->values.m11 * matrix->values.m34 * matrix->values.m42 +
				 matrix->values.m31 * matrix->values.m12 * matrix->values.m44 -
				 matrix->values.m31 * matrix->values.m14 * matrix->values.m42 -
				 matrix->values.m41 * matrix->values.m12 * matrix->values.m34 +
				 matrix->values.m41 * matrix->values.m14 * matrix->values.m32;

		inv[13] = matrix->values.m11 * matrix->values.m32 * matrix->values.m43 -
				  matrix->values.m11 * matrix->values.m33 * matrix->values.m42 -
				  matrix->values.m31 * matrix->values.m12 * matrix->values.m43 +
				  matrix->values.m31 * matrix->values.m13 * matrix->values.m42 +
				  matrix->values.m41 * matrix->values.m12 * matrix->values.m33 -
				  matrix->values.m41 * matrix->values.m13 * matrix->values.m32;

		inv[2] = matrix->values.m12 * matrix->values.m23 * matrix->values.m44 -
				 matrix->values.m12 * matrix->values.m24 * matrix->values.m43 -
				 matrix->values.m22 * matrix->values.m13 * matrix->values.m44 +
				 matrix->values.m22 * matrix->values.m14 * matrix->values.m43 +
				 matrix->values.m42 * matrix->values.m13 * matrix->values.m24 -
				 matrix->values.m42 * matrix->values.m14 * matrix->values.m23;

		inv[6] = -matrix->values.m11 * matrix->values.m23 * matrix->values.m44 +
				 matrix->values.m11 * matrix->values.m24 * matrix->values.m43 +
				 matrix->values.m21 * matrix->values.m13 * matrix->values.m44 -
				 matrix->values.m21 * matrix->values.m14 * matrix->values.m43 -
				 matrix->values.m41 * matrix->values.m13 * matrix->values.m24 +
				 matrix->values.m41 * matrix->values.m14 * matrix->values.m23;

		inv[10] = matrix->values.m11 * matrix->values.m22 * matrix->values.m44 -
				  matrix->values.m11 * matrix->values.m24 * matrix->values.m42 -
				  matrix->values.m21 * matrix->values.m12 * matrix->values.m44 +
				  matrix->values.m21 * matrix->values.m14 * matrix->values.m42 +
				  matrix->values.m41 * matrix->values.m12 * matrix->values.m24 -
				  matrix->values.m41 * matrix->values.m14 * matrix->values.m22;

		inv[14] =
			-matrix->values.m11 * matrix->values.m22 * matrix->values.m43 +
			matrix->values.m11 * matrix->values.m23 * matrix->values.m42 +
			matrix->values.m21 * matrix->values.m12 * matrix->values.m43 -
			matrix->values.m21 * matrix->values.m13 * matrix->values.m42 -
			matrix->values.m41 * matrix->values.m12 * matrix->values.m23 +
			matrix->values.m41 * matrix->values.m13 * matrix->values.m22;

		inv[3] = -matrix->values.m12 * matrix->values.m23 * matrix->values.m34 +
				 matrix->values.m12 * matrix->values.m24 * matrix->values.m33 +
				 matrix->values.m22 * matrix->values.m13 * matrix->values.m34 -
				 matrix->values.m22 * matrix->values.m14 * matrix->values.m33 -
				 matrix->values.m32 * matrix->values.m13 * matrix->values.m24 +
				 matrix->values.m32 * matrix->values.m14 * matrix->values.m23;

		inv[7] = matrix->values.m11 * matrix->values.m23 * matrix->values.m34 -
				 matrix->values.m11 * matrix->values.m24 * matrix->values.m33 -
				 matrix->values.m21 * matrix->values.m13 * matrix->values.m34 +
				 matrix->values.m21 * matrix->values.m14 * matrix->values.m33 +
				 matrix->values.m31 * matrix->values.m13 * matrix->values.m24 -
				 matrix->values.m31 * matrix->values.m14 * matrix->values.m23;

		inv[11] =
			-matrix->values.m11 * matrix->values.m22 * matrix->values.m34 +
			matrix->values.m11 * matrix->values.m24 * matrix->values.m32 +
			matrix->values.m21 * matrix->values.m12 * matrix->values.m34 -
			matrix->values.m21 * matrix->values.m14 * matrix->values.m32 -
			matrix->values.m31 * matrix->values.m12 * matrix->values.m24 +
			matrix->values.m31 * matrix->values.m14 * matrix->values.m22;

		inv[15] = matrix->values.m11 * matrix->values.m22 * matrix->values.m33 -
				  matrix->values.m11 * matrix->values.m23 * matrix->values.m32 -
				  matrix->values.m21 * matrix->values.m12 * matrix->values.m33 +
				  matrix->values.m21 * matrix->values.m13 * matrix->values.m32 +
				  matrix->values.m31 * matrix->values.m12 * matrix->values.m23 -
				  matrix->values.m31 * matrix->values.m13 * matrix->values.m22;

		matrix->values.m11 = inv[0] / det;
		matrix->values.m12 = inv[1] / det;
		matrix->values.m13 = inv[2] / det;
		matrix->values.m14 = inv[3] / det;
		matrix->values.m21 = inv[4] / det;
		matrix->values.m22 = inv[5] / det;
		matrix->values.m23 = inv[6] / det;
		matrix->values.m24 = inv[7] / det;
		matrix->values.m31 = inv[8] / det;
		matrix->values.m32 = inv[9] / det;
		matrix->values.m33 = inv[10] / det;
		matrix->values.m34 = inv[11] / det;
		matrix->values.m41 = inv[12] / det;
		matrix->values.m42 = inv[13] / det;
		matrix->values.m43 = inv[14] / det;
		matrix->values.m44 = inv[15] / det;
	}
}

static JSValue nx_dommatrix_invert_self(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	nx_dommatrix_invert_self_(matrix);
	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_multiply_self(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	if (argc == 1 && JS_IsObject(argv[0])) {
		nx_dommatrix_t *other = nx_get_dommatrix(ctx, argv[0]);
		bool needs_free = false;
		if (!other) {
			other = js_malloc(ctx, sizeof(nx_dommatrix_t));
			needs_free = true;
			if (nx_dommatrix_init(ctx, argv[0], other)) {
				js_free(ctx, other);
				return JS_EXCEPTION;
			}
		}

		nx_dommatrix_values_t result;
		multiply(&other->values, &matrix->values, &result);
		*(&matrix->values) = result;

		if (needs_free) {
			js_free(ctx, other);
		}
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_premultiply_self(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	DOMMATRIX_THIS;
	if (argc == 1 && JS_IsObject(argv[0])) {
		nx_dommatrix_t *other = nx_get_dommatrix(ctx, argv[0]);
		bool needs_free = false;
		if (!other) {
			other = js_malloc(ctx, sizeof(nx_dommatrix_t));
			needs_free = true;
			if (nx_dommatrix_init(ctx, argv[0], other)) {
				js_free(ctx, other);
				return JS_EXCEPTION;
			}
		}

		nx_dommatrix_values_t result;
		multiply(&matrix->values, &other->values, &result);
		*(&matrix->values) = result;

		if (needs_free) {
			js_free(ctx, other);
		}
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_rotate_axis_angle_self(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	DOMMATRIX_THIS;
	double x = 0, y = 0, z = 0, angle = 0;
	ARG_TO_NUM(0, x);
	ARG_TO_NUM(1, y);
	ARG_TO_NUM(2, z);
	ARG_TO_NUM(3, angle);

	// Normalize axis
	double length = sqrt(x * x + y * y + z * z);
	if (length == 0.) {
		return JS_DupValue(ctx, this_val);
	}
	if (length != 1.) {
		x /= length;
		y /= length;
		z /= length;
	}
	angle *= RADS_PER_DEGREE;
	double c = cos(angle);
	double s = sin(angle);
	double t = 1. - c;
	double tx = t * x;
	double ty = t * y;

	// NB: This is the generic transform. If the axis is a major axis, there are
	// faster transforms.

	nx_dommatrix_values_t b;
	nx_dommatrix_values_t result;
	double values[] = {tx * x + c,
					   tx * y + s * z,
					   tx * z - s * y,
					   0,
					   tx * y - s * z,
					   ty * y + c,
					   ty * z + s * x,
					   0,
					   tx * z + s * y,
					   ty * z - s * x,
					   t * z * z + c,
					   0,
					   0,
					   0,
					   0,
					   1};
	matrix_values(&b, values);
	multiply(&b, &matrix->values, &result);
	*(&matrix->values) = result;

	if (x != 0 || y != 0) {
		matrix->is_2d = false;
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_rotate_self(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	double rotX = 0;
	double rotY = 0;
	double rotZ = 0;
	ARG_TO_NUM(0, rotX);

	bool rotYIsNum = ARG_IS_NUM(1);
	bool rotZIsNum = ARG_IS_NUM(2);
	if (!rotYIsNum && !rotZIsNum) {
		rotZ = rotX;
		rotX = 0;
	} else {
		ARG_TO_NUM(1, rotY);
		ARG_TO_NUM(2, rotZ);
	}

	if (rotX != 0 || rotY != 0) {
		matrix->is_2d = false;
	}

	rotX *= RADS_PER_DEGREE;
	rotY *= RADS_PER_DEGREE;
	rotZ *= RADS_PER_DEGREE;

	double c, s;
	nx_dommatrix_values_t result;
	nx_dommatrix_values_t b;

	c = cos(rotZ);
	s = sin(rotZ);
	double valuesZ[] = {
		c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
	};
	matrix_values(&b, valuesZ);
	multiply(&b, &matrix->values, &result);
	*(&matrix->values) = result;

	c = cos(rotY);
	s = sin(rotY);
	double valuesY[] = {
		c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1,
	};
	matrix_values(&b, valuesY);
	multiply(&b, &matrix->values, &result);
	*(&matrix->values) = result;

	c = cos(rotX);
	s = sin(rotX);
	double valuesX[] = {
		1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1,
	};
	matrix_values(&b, valuesX);
	multiply(&b, &matrix->values, &result);
	*(&matrix->values) = result;

	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_scale_self(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	double originX = 0, originY = 0, originZ = 0;
	ARG_TO_NUM(3, originX);
	ARG_TO_NUM(4, originY);
	ARG_TO_NUM(5, originZ);
	translate(matrix, originX, originY, originZ);

	double scaleX = 1;
	ARG_TO_NUM(0, scaleX);

	double scaleY = scaleX;
	ARG_TO_NUM(1, scaleY);

	double scaleZ = 1;
	ARG_TO_NUM(2, scaleZ);

	nx_dommatrix_values_t result;
	nx_dommatrix_values_t b;
	double values[] = {
		scaleX, 0, 0, 0, 0, scaleY, 0, 0, 0, 0, scaleZ, 0, 0, 0, 0, 1,
	};
	matrix_values(&b, values);
	multiply(&b, &matrix->values, &result);
	*(&matrix->values) = result;

	translate(matrix, -originX, -originY, -originZ);

	if (scaleZ != 1 || originZ != 0) {
		matrix->is_2d = false;
	}

	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_skew_x_self(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	double sx = 0;
	ARG_TO_NUM(0, sx);
	if (sx != 0) {
		double t = tan(sx * RADS_PER_DEGREE);
		nx_dommatrix_values_t result;
		nx_dommatrix_values_t b;
		double values[] = {
			1, 0, 0, 0, t, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
		};
		matrix_values(&b, values);
		multiply(&b, &matrix->values, &result);
		*(&matrix->values) = result;
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_skew_y_self(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	DOMMATRIX_THIS;
	double sy = 0;
	ARG_TO_NUM(0, sy);
	if (sy != 0) {
		double t = tan(sy * RADS_PER_DEGREE);
		nx_dommatrix_values_t result;
		nx_dommatrix_values_t b;
		double values[] = {
			1, t, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
		};
		matrix_values(&b, values);
		multiply(&b, &matrix->values, &result);
		*(&matrix->values) = result;
	}
	return JS_DupValue(ctx, this_val);
}

static JSValue nx_dommatrix_translate_self(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	DOMMATRIX_THIS;
	double tx = 0;
	double ty = 0;
	double tz = 0;
	ARG_TO_NUM(0, tx);
	ARG_TO_NUM(1, ty);
	ARG_TO_NUM(2, tz);
	translate(matrix, tx, ty, tz);
	return JS_DupValue(ctx, this_val);
}

void nx_dommatrix_transform_point_(nx_dommatrix_t *matrix, double *x, double *y,
								   double *z, double *w) {
	double nx = matrix->values.m11 * (*x) + matrix->values.m21 * (*y) +
				matrix->values.m31 * (*z) + matrix->values.m41 * (*w);

	double ny = matrix->values.m12 * (*x) + matrix->values.m22 * (*y) +
				matrix->values.m32 * (*z) + matrix->values.m42 * (*w);

	double nz = matrix->values.m13 * (*x) + matrix->values.m23 * (*y) +
				matrix->values.m33 * (*z) + matrix->values.m43 * (*w);

	double nw = matrix->values.m14 * (*x) + matrix->values.m24 * (*y) +
				matrix->values.m34 * (*z) + matrix->values.m44 * (*w);

	*x = nx;
	*y = ny;
	*z = nz;
	*w = nw;
}

static JSValue nx_dommatrix_transform_point(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_dommatrix_t *matrix = JS_GetOpaque2(ctx, argv[0], nx_dommatrix_class_id);
	if (!matrix) {
		return JS_EXCEPTION;
	}
	double x = 0, y = 0, z = 0, w = 1;
	JSValue v;
	v = JS_GetPropertyStr(ctx, argv[1], "x");
	if (JS_IsNumber(v))
		JS_ToFloat64(ctx, &x, v);
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[1], "y");
	if (JS_IsNumber(v))
		JS_ToFloat64(ctx, &y, v);
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[1], "z");
	if (JS_IsNumber(v))
		JS_ToFloat64(ctx, &z, v);
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, argv[1], "w");
	if (JS_IsNumber(v))
		JS_ToFloat64(ctx, &w, v);
	JS_FreeValue(ctx, v);

	nx_dommatrix_transform_point_(matrix, &x, &y, &z, &w);

	JSValue point = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, point, "x", JS_NewFloat64(ctx, x));
	JS_SetPropertyStr(ctx, point, "y", JS_NewFloat64(ctx, y));
	JS_SetPropertyStr(ctx, point, "z", JS_NewFloat64(ctx, z));
	JS_SetPropertyStr(ctx, point, "w", JS_NewFloat64(ctx, w));
	return point;
}

static JSValue nx_dommatrix_read_only_init_class(JSContext *ctx,
												 JSValueConst this_val,
												 int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "a", nx_dommatrix_get_m11);
	NX_DEF_GET(proto, "b", nx_dommatrix_get_m12);
	NX_DEF_GET(proto, "c", nx_dommatrix_get_m21);
	NX_DEF_GET(proto, "d", nx_dommatrix_get_m22);
	NX_DEF_GET(proto, "e", nx_dommatrix_get_m41);
	NX_DEF_GET(proto, "f", nx_dommatrix_get_m42);
	NX_DEF_GET(proto, "m11", nx_dommatrix_get_m11);
	NX_DEF_GET(proto, "m12", nx_dommatrix_get_m12);
	NX_DEF_GET(proto, "m13", nx_dommatrix_get_m13);
	NX_DEF_GET(proto, "m14", nx_dommatrix_get_m14);
	NX_DEF_GET(proto, "m21", nx_dommatrix_get_m21);
	NX_DEF_GET(proto, "m22", nx_dommatrix_get_m22);
	NX_DEF_GET(proto, "m23", nx_dommatrix_get_m23);
	NX_DEF_GET(proto, "m24", nx_dommatrix_get_m24);
	NX_DEF_GET(proto, "m31", nx_dommatrix_get_m31);
	NX_DEF_GET(proto, "m32", nx_dommatrix_get_m32);
	NX_DEF_GET(proto, "m33", nx_dommatrix_get_m33);
	NX_DEF_GET(proto, "m34", nx_dommatrix_get_m34);
	NX_DEF_GET(proto, "m41", nx_dommatrix_get_m41);
	NX_DEF_GET(proto, "m42", nx_dommatrix_get_m42);
	NX_DEF_GET(proto, "m43", nx_dommatrix_get_m43);
	NX_DEF_GET(proto, "m44", nx_dommatrix_get_m44);
	NX_DEF_GET(proto, "is2D", nx_dommatrix_is_2d);
	NX_DEF_GET(proto, "isIdentity", nx_dommatrix_is_identity);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_dommatrix_init_class(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GETSET(proto, "a", nx_dommatrix_get_m11, nx_dommatrix_set_m11);
	NX_DEF_GETSET(proto, "b", nx_dommatrix_get_m12, nx_dommatrix_set_m12);
	NX_DEF_GETSET(proto, "c", nx_dommatrix_get_m21, nx_dommatrix_set_m21);
	NX_DEF_GETSET(proto, "d", nx_dommatrix_get_m22, nx_dommatrix_set_m22);
	NX_DEF_GETSET(proto, "e", nx_dommatrix_get_m41, nx_dommatrix_set_m41);
	NX_DEF_GETSET(proto, "f", nx_dommatrix_get_m42, nx_dommatrix_set_m42);
	NX_DEF_GETSET(proto, "m11", nx_dommatrix_get_m11, nx_dommatrix_set_m11);
	NX_DEF_GETSET(proto, "m12", nx_dommatrix_get_m12, nx_dommatrix_set_m12);
	NX_DEF_GETSET(proto, "m13", nx_dommatrix_get_m13, nx_dommatrix_set_m13);
	NX_DEF_GETSET(proto, "m14", nx_dommatrix_get_m14, nx_dommatrix_set_m14);
	NX_DEF_GETSET(proto, "m21", nx_dommatrix_get_m21, nx_dommatrix_set_m21);
	NX_DEF_GETSET(proto, "m22", nx_dommatrix_get_m22, nx_dommatrix_set_m22);
	NX_DEF_GETSET(proto, "m23", nx_dommatrix_get_m23, nx_dommatrix_set_m23);
	NX_DEF_GETSET(proto, "m24", nx_dommatrix_get_m24, nx_dommatrix_set_m24);
	NX_DEF_GETSET(proto, "m31", nx_dommatrix_get_m31, nx_dommatrix_set_m31);
	NX_DEF_GETSET(proto, "m32", nx_dommatrix_get_m32, nx_dommatrix_set_m32);
	NX_DEF_GETSET(proto, "m33", nx_dommatrix_get_m33, nx_dommatrix_set_m33);
	NX_DEF_GETSET(proto, "m34", nx_dommatrix_get_m34, nx_dommatrix_set_m34);
	NX_DEF_GETSET(proto, "m41", nx_dommatrix_get_m41, nx_dommatrix_set_m41);
	NX_DEF_GETSET(proto, "m42", nx_dommatrix_get_m42, nx_dommatrix_set_m42);
	NX_DEF_GETSET(proto, "m43", nx_dommatrix_get_m43, nx_dommatrix_set_m43);
	NX_DEF_GETSET(proto, "m44", nx_dommatrix_get_m44, nx_dommatrix_set_m44);
	NX_DEF_FUNC(proto, "invertSelf", nx_dommatrix_invert_self, 0);
	NX_DEF_FUNC(proto, "multiplySelf", nx_dommatrix_multiply_self, 0);
	NX_DEF_FUNC(proto, "preMultiplySelf", nx_dommatrix_premultiply_self, 0);
	NX_DEF_FUNC(proto, "rotateAxisAngleSelf",
				nx_dommatrix_rotate_axis_angle_self, 0);
	NX_DEF_FUNC(proto, "rotateSelf", nx_dommatrix_rotate_self, 0);
	NX_DEF_FUNC(proto, "scaleSelf", nx_dommatrix_scale_self, 0);
	NX_DEF_FUNC(proto, "skewXSelf", nx_dommatrix_skew_x_self, 0);
	NX_DEF_FUNC(proto, "skewYSelf", nx_dommatrix_skew_y_self, 0);
	NX_DEF_FUNC(proto, "translateSelf", nx_dommatrix_translate_self, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static void finalizer_dommatrix(JSRuntime *rt, JSValue val) {
	nx_dommatrix_t *m = JS_GetOpaque(val, nx_dommatrix_class_id);
	if (m) {
		js_free_rt(rt, m);
	}
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("dommatrixNew", 0, nx_dommatrix_new),
	JS_CFUNC_DEF("dommatrixFromMatrix", 0, nx_dommatrix_from_matrix),
	JS_CFUNC_DEF("dommatrixROInitClass", 0, nx_dommatrix_read_only_init_class),
	JS_CFUNC_DEF("dommatrixInitClass", 0, nx_dommatrix_init_class),
	JS_CFUNC_DEF("dommatrixTransformPoint", 0, nx_dommatrix_transform_point),
};

void nx_init_dommatrix(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_dommatrix_class_id);
	JSClassDef dommatrix_class = {
		"DOMMatrix",
		.finalizer = finalizer_dommatrix,
	};
	JS_NewClass(rt, nx_dommatrix_class_id, &dommatrix_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
