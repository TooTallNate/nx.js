#include "util.h"

u8 *NX_GetArrayBufferView(JSContext *ctx, size_t *size, JSValueConst obj) {
	if (!JS_IsObject(obj)) {
		return NULL;
	}
	if (JS_IsArrayBuffer(obj)) {
		return JS_GetArrayBuffer(ctx, size, obj);
	}
	JSValue buffer_val = JS_GetPropertyStr(ctx, obj, "buffer");
	if (!JS_IsArrayBuffer(buffer_val)) {
		return NULL;
	}
	u32 byte_offset;
	u32 byte_length;
	JSValue byte_offset_val = JS_GetPropertyStr(ctx, obj, "byteOffset");
	JSValue byte_length_val = JS_GetPropertyStr(ctx, obj, "byteLength");
	if (JS_ToUint32(ctx, &byte_offset, byte_offset_val) ||
		JS_ToUint32(ctx, &byte_length, byte_length_val)) {
		return NULL;
	}
	size_t ab_size; // not used
	u8 *ptr = JS_GetArrayBuffer(ctx, &ab_size, buffer_val);
	if (!ptr) {
		return NULL;
	}
	*size = byte_length;
	return ptr + byte_offset;
}
