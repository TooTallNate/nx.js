#pragma once
#include "types.h"
#include "include/core/SkPath.h"

// A Path2D is an SkPath built in user space (the canvas CTM is applied later,
// when the path is used via ctx.fill/stroke/clip/isPointInPath). See path2d.cc.
typedef struct nx_path2d_s nx_path2d_t;

// Return the user-space SkPath backing a Path2D JS object, or nullptr if `obj`
// is not a Path2D. The returned path is owned by the Path2D; copy if retaining.
const SkPath *nx_path2d_get(v8::Isolate *iso, v8::Local<v8::Value> obj);

void nx_init_path2d(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
