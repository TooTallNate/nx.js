#pragma once
#include "types.h"
#include <cairo.h>

typedef struct {
	// Same order as cairo_matrix_t for the 2D subset.
	double m11, m12, m21, m22, m41, m42;
	double m13, m14, m23, m24, m31, m32, m33, m34, m43, m44;
} nx_dommatrix_values_t;

typedef struct {
	union {
		nx_dommatrix_values_t values;
		cairo_matrix_t cr_matrix;
	};
	bool is_2d;
} nx_dommatrix_t;

nx_dommatrix_t *nx_get_dommatrix(v8::Isolate *iso, v8::Local<v8::Value> obj);
// Populate `matrix` from a DOMMatrixInit-like JS object. Returns 0 on success.
int nx_dommatrix_init(v8::Isolate *iso, v8::Local<v8::Value> obj,
                      nx_dommatrix_t *matrix);
bool nx_dommatrix_is_identity_(nx_dommatrix_t *matrix);
void nx_dommatrix_invert_self_(nx_dommatrix_t *matrix);
void nx_dommatrix_transform_point_(nx_dommatrix_t *matrix, double *x, double *y,
                                   double *z, double *w);

void nx_init_dommatrix(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
