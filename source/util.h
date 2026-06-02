#pragma once
#include "types.h"

// Extract the raw bytes of an ArrayBuffer or ArrayBufferView (TypedArray /
// DataView), honoring byteOffset/byteLength. Returns NULL if `obj` is neither.
// The returned pointer is owned by the backing store; it is valid as long as
// the ArrayBuffer is alive.
uint8_t *NX_GetBufferSource(v8::Isolate *iso, size_t *size,
                            v8::Local<v8::Value> obj);

// In-place file-extension replacement (e.g. "/foo/bar.nro" -> "/foo/bar.js").
void replace_file_extension(char *path, const char *new_extension);
