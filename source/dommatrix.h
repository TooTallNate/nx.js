#pragma once
#include <cairo.h>
#include "types.h"

typedef struct
{
	// It's important that these are in the same order as `cairo_matrix_t`
	double m11; // a / xx
	double m12; // b / yx
	double m21; // c / xy
	double m22; // d / yy
	double m41; // e / x0
	double m42; // f / y0

	double m13;
	double m14;
	double m23;
	double m24;
	double m31;
	double m32;
	double m33;
	double m34;
	double m43;
	double m44;
} nx_dommatrix_values_t;

typedef struct
{
	union
	{
		nx_dommatrix_values_t values;
		cairo_matrix_t cr_matrix;
	};
	bool is_2d;
} nx_dommatrix_t;

nx_dommatrix_t *nx_get_dommatrix(JSContext *ctx, JSValueConst obj);
int nx_dommatrix_init(JSContext *ctx, JSValueConst obj, nx_dommatrix_t *matrix);
bool nx_dommatrix_is_identity_(nx_dommatrix_t* matrix);
void nx_dommatrix_invert_self_(nx_dommatrix_t* matrix);
void nx_dommatrix_transform_point_(nx_dommatrix_t *matrix, double *x, double *y, double *z, double *w);

void nx_init_dommatrix(JSContext *ctx, JSValueConst init_obj);
