#include "util.h"
#include <string.h>

using namespace v8;

// A valid buffer source can legitimately be EMPTY (e.g. a zero-length
// Uint8Array — V8 backs these with a null data pointer). Callers conventionally
// test the returned pointer for null to mean "not a buffer source", so for an
// empty-but-valid buffer we return this non-null sentinel with *size == 0,
// letting callers distinguish "empty" from "wrong type". Never dereferenced
// (size is 0).
static uint8_t NX_EMPTY_BUFFER_SENTINEL = 0;

uint8_t *NX_GetBufferSource(Isolate *iso, size_t *size, Local<Value> obj) {
	if (obj.IsEmpty() || !obj->IsObject()) {
		return nullptr;
	}

	if (obj->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = obj.As<ArrayBuffer>();
		*size = ab->ByteLength();
		uint8_t *data = static_cast<uint8_t *>(ab->Data());
		return data ? data : &NX_EMPTY_BUFFER_SENTINEL;
	}

	if (obj->IsArrayBufferView()) {
		Local<ArrayBufferView> view = obj.As<ArrayBufferView>();
		Local<ArrayBuffer> ab = view->Buffer();
		*size = view->ByteLength();
		uint8_t *data = static_cast<uint8_t *>(ab->Data());
		if (!data) {
			return &NX_EMPTY_BUFFER_SENTINEL;
		}
		return data + view->ByteOffset();
	}

	return nullptr;
}

void replace_file_extension(char *path, const char *new_extension) {
	char *dot = strrchr(path, '.');
	char *slash = strrchr(path, '/');

	// Only replace the extension if a dot is found after the last separator.
	if (dot != nullptr && (slash == nullptr || dot > slash)) {
		strcpy(dot, new_extension);
	}
}
