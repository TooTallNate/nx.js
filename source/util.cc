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

	// Duck-typed buffer source: any object exposing `{ buffer, byteOffset,
	// byteLength }` where `buffer` is an ArrayBuffer. This is the shape of
	// `@nx.js/util`'s `ArrayBufferStruct` (used pervasively by @nx.js/ncm and
	// other packages to describe IPC structs), which is a plain JS class — NOT
	// a real V8 ArrayBufferView, so it falls through the checks above. The
	// QuickJS-era NX_GetBufferSource duck-typed these; the V8 rewrite dropped
	// it, which silently returned nullptr here and produced malformed service
	// IPC requests (kernel ConnectionClosed) for any command passed such a
	// struct. Restore the fallback.
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> o = obj.As<Object>();
	Local<Value> buffer_val;
	if (o->Get(ctx, nx_str(iso, "buffer")).ToLocal(&buffer_val) &&
	    buffer_val->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = buffer_val.As<ArrayBuffer>();
		size_t ab_size = ab->ByteLength();
		uint8_t *base = static_cast<uint8_t *>(ab->Data());

		// byteOffset / byteLength (default to 0 / whole buffer).
		uint32_t byte_offset = 0, byte_length = (uint32_t)ab_size;
		Local<Value> v;
		if (o->Get(ctx, nx_str(iso, "byteOffset")).ToLocal(&v) && v->IsNumber())
			byte_offset = v->Uint32Value(ctx).FromMaybe(0);
		if (o->Get(ctx, nx_str(iso, "byteLength")).ToLocal(&v) && v->IsNumber())
			byte_length = v->Uint32Value(ctx).FromMaybe(byte_length);

		// Clamp to the backing buffer so a bogus offset/length can't escape it.
		if (byte_offset > ab_size)
			byte_offset = (uint32_t)ab_size;
		if ((size_t)byte_offset + byte_length > ab_size)
			byte_length = (uint32_t)(ab_size - byte_offset);

		*size = byte_length;
		if (!base || byte_length == 0) {
			return &NX_EMPTY_BUFFER_SENTINEL;
		}
		return base + byte_offset;
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
