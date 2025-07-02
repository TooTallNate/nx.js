#pragma once
#include "types.h"

typedef struct {
	bool is_nro;
	void *icon;
	char *nro_path;
	size_t icon_size;
	NacpStruct nacp;
} nx_app_t;

nx_app_t *nx_get_app(JSContext *ctx, JSValueConst obj);

void nx_init_ns(JSContext *ctx, JSValueConst init_obj);
