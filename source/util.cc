#include "util.h"
#include <string.h>

using namespace v8;

uint8_t *NX_GetBufferSource(Isolate *iso, size_t *size, Local<Value> obj) {
	if (obj.IsEmpty() || !obj->IsObject()) {
		return nullptr;
	}

	if (obj->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = obj.As<ArrayBuffer>();
		*size = ab->ByteLength();
		return static_cast<uint8_t *>(ab->Data());
	}

	if (obj->IsArrayBufferView()) {
		Local<ArrayBufferView> view = obj.As<ArrayBufferView>();
		Local<ArrayBuffer> ab = view->Buffer();
		*size = view->ByteLength();
		return static_cast<uint8_t *>(ab->Data()) + view->ByteOffset();
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
