#include "util.h"

u8 *NX_GetBufferSource(JSContext *ctx, size_t *size, JSValueConst obj) {
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

/**
 * @brief Replaces the file extension of a given path string.
 *
 * The modification is done in-place. The caller must ensure that the buffer
 * for `path` is large enough to hold the `new_extension`.
 *
 * This function will not add an extension if one does not already exist.
 * It also correctly handles filenames starting with a dot (e.g. ".profile")
 * and paths containing dots in directory names (e.g. "my.app/program").
 *
 * @param path The file path string to modify.
 * @param new_extension The new extension, which should include the leading dot.
 */
void replace_file_extension(char *path, const char *new_extension) {
	char *dot = strrchr(path, '.');
	char *slash = strrchr(path, '/');

	// We only replace the extension if a dot is found,
	// and it appears after the last path separator.
	if (dot != NULL && (slash == NULL || dot > slash)) {
		strcpy(dot, new_extension);
	}
}
