#include "error.h"
#include "types.h"
#include <mbedtls/base64.h>
#include <stdlib.h>

using namespace v8;

namespace {

void nx_atob(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value input(iso, info[0]);
	if (!*input) {
		return;
	}
	size_t input_len = input.length();
	if (input_len == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}
	size_t output_len = 0;
	int ret = mbedtls_base64_decode(NULL, 0, &output_len,
	                                (const unsigned char *)*input, input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		iso->ThrowException(
		    Exception::SyntaxError(nx_str(iso, "Invalid base64 character")));
		return;
	}
	uint8_t *output = (uint8_t *)malloc(output_len + 1);
	ret = mbedtls_base64_decode(output, output_len, &output_len,
	                            (const unsigned char *)*input, input_len);
	if (ret != 0) {
		free(output);
		iso->ThrowException(
		    Exception::SyntaxError(nx_str(iso, "Invalid base64 character")));
		return;
	}
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, (char *)output, NewStringType::kNormal,
	                        (int)output_len)
	        .ToLocalChecked());
	free(output);
}

void nx_btoa(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value input(iso, info[0]);
	if (!*input) {
		return;
	}
	size_t input_len = input.length();
	if (input_len == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}
	size_t output_len = 0;
	int ret = mbedtls_base64_encode(NULL, 0, &output_len,
	                                (const unsigned char *)*input, input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		nx_throw(iso, "base64 encode failed");
		return;
	}
	unsigned char *output = (unsigned char *)malloc(output_len);
	ret = mbedtls_base64_encode(output, output_len, &output_len,
	                            (const unsigned char *)*input, input_len);
	if (ret != 0) {
		free(output);
		nx_throw(iso, "base64 encode failed");
		return;
	}
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, (char *)output, NewStringType::kNormal,
	                        (int)output_len)
	        .ToLocalChecked());
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
