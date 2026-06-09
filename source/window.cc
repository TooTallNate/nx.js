#include "error.h"
#include "types.h"
#include <mbedtls/base64.h>
#include <stdlib.h>

using namespace v8;

namespace {

// Throw a DOMException-flavored InvalidCharacterError. The HTML spec requires
// atob/btoa to throw "InvalidCharacterError" (a DOMException) on bad input.
// We don't have a native DOMException constructor here, so throw an Error whose
// name is set to "InvalidCharacterError" — `instanceof`/`.name` checks and the
// message match what callers expect.
static void throw_invalid_character(Isolate *iso, const char *msg) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> err = Exception::Error(nx_str(iso, msg));
	if (err->IsObject()) {
		err.As<Object>()
		    ->Set(ctx, nx_str(iso, "name"),
		          nx_str(iso, "InvalidCharacterError"))
		    .Check();
	}
	iso->ThrowException(err);
}

// atob(): decode a base64 string into a "binary string" — each output char is a
// byte in 0..255 (Latin-1), NOT UTF-8. Using a one-byte (Latin-1) string is the
// whole point: a previous UTF-8 round-trip mangled any decoded byte >= 0x80
// (e.g. 0xFF), so atob() couldn't be used to recover raw bytes.
void nx_atob(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	// Per spec the argument is stringified (ToString) first.
	Local<String> in_str;
	if (!info[0]->ToString(iso->GetCurrentContext()).ToLocal(&in_str)) {
		return; // pending exception from coercion
	}

	// Read the input as Latin-1 bytes. Per the forgiving-base64 algorithm the
	// input itself is ASCII base64 (one-byte), so WriteOneByte is correct.
	int input_len = in_str->Length();
	if (input_len == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}
	uint8_t *input = (uint8_t *)malloc((size_t)input_len + 1);
	if (!input) {
		nx_throw(iso, "out of memory");
		return;
	}
	in_str->WriteOneByte(iso, 0, input_len, input);
	input[input_len] = '\0';

	size_t output_len = 0;
	int ret = mbedtls_base64_decode(NULL, 0, &output_len, input,
	                                (size_t)input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		free(input);
		throw_invalid_character(iso,
		                        "The string to be decoded is not correctly "
		                        "encoded.");
		return;
	}
	uint8_t *output = (uint8_t *)malloc(output_len + 1);
	if (!output) {
		free(input);
		nx_throw(iso, "out of memory");
		return;
	}
	ret = mbedtls_base64_decode(output, output_len, &output_len, input,
	                            (size_t)input_len);
	free(input);
	if (ret != 0) {
		free(output);
		throw_invalid_character(iso,
		                        "The string to be decoded is not correctly "
		                        "encoded.");
		return;
	}
	// Return a Latin-1 (one-byte) string: each output char is a raw byte.
	Local<String> result;
	if (String::NewFromOneByte(iso, output, NewStringType::kNormal,
	                           (int)output_len)
	        .ToLocal(&result)) {
		info.GetReturnValue().Set(result);
	}
	free(output);
}

// btoa(): encode a "binary string" to base64. Each input char must be a byte
// in 0..255; a char > 255 (i.e. the string isn't a Latin-1 binary string) is an
// InvalidCharacterError per spec. We read the input as Latin-1 bytes after
// validating it contains only one-byte code points.
void nx_btoa(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<String> in_str;
	if (!info[0]->ToString(iso->GetCurrentContext()).ToLocal(&in_str)) {
		return; // pending exception from coercion
	}

	int input_len = in_str->Length();
	if (input_len == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}

	// Spec: btoa throws InvalidCharacterError if any code unit is > 0xFF.
	// ContainsOnlyOneByte() reads the string and is exact (no false positives).
	if (!in_str->ContainsOnlyOneByte()) {
		throw_invalid_character(
		    iso, "The string to be encoded contains characters outside of the "
		         "Latin1 range.");
		return;
	}

	uint8_t *input = (uint8_t *)malloc((size_t)input_len);
	if (!input) {
		nx_throw(iso, "out of memory");
		return;
	}
	in_str->WriteOneByte(iso, 0, input_len, input);

	size_t output_len = 0;
	int ret = mbedtls_base64_encode(NULL, 0, &output_len, input,
	                                (size_t)input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		free(input);
		nx_throw(iso, "base64 encode failed");
		return;
	}
	unsigned char *output = (unsigned char *)malloc(output_len);
	if (!output) {
		free(input);
		nx_throw(iso, "out of memory");
		return;
	}
	ret = mbedtls_base64_encode(output, output_len, &output_len, input,
	                            (size_t)input_len);
	free(input);
	if (ret != 0) {
		free(output);
		nx_throw(iso, "base64 encode failed");
		return;
	}
	// base64 output is ASCII; a one-byte string is fine.
	Local<String> result;
	if (String::NewFromOneByte(iso, output, NewStringType::kNormal,
	                           (int)output_len)
	        .ToLocal(&result)) {
		info.GetReturnValue().Set(result);
	}
	free(output);
}

void nx_window_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> target = info[0].As<Object>();
	NX_DEF_FUNC(target, "atob", nx_atob, 1);
	NX_DEF_FUNC(target, "btoa", nx_btoa, 1);
}

} // namespace

void nx_init_window(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "windowInit", nx_window_init);
}
