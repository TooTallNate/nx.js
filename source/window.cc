#include "error.h"
#include "types.h"
#include <mbedtls/base64.h>
#include <stdlib.h>

using namespace v8;

namespace {

// Throw an "InvalidCharacterError" — the DOMException the HTML spec requires
// atob/btoa to throw on bad input. Prefer the real global `DOMException`
// (so `instanceof DOMException` works, matching Chrome/WebIDL):
// `new DOMException(msg, "InvalidCharacterError")`. Fall back to an Error whose
// `name` is set if the constructor isn't available for any reason.
static void throw_invalid_character(Isolate *iso, const char *msg) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> global = ctx->Global();
	Local<Value> de_ctor_v;
	if (global->Get(ctx, nx_str(iso, "DOMException")).ToLocal(&de_ctor_v) &&
	    de_ctor_v->IsFunction()) {
		Local<Function> de_ctor = de_ctor_v.As<Function>();
		Local<Value> args[] = {nx_str(iso, msg),
		                       nx_str(iso, "InvalidCharacterError")};
		Local<Value> ex;
		if (de_ctor->NewInstance(ctx, 2, args).ToLocal(&ex)) {
			iso->ThrowException(ex);
			return;
		}
	}
	// Fallback: an Error with the right name.
	Local<Value> err = Exception::Error(nx_str(iso, msg));
	if (err->IsObject()) {
		err.As<Object>()
		    ->Set(ctx, nx_str(iso, "name"),
		          nx_str(iso, "InvalidCharacterError"))
		    .Check();
	}
	iso->ThrowException(err);
}

// base64 alphabet -> 6-bit value; -1 = not a base64 char.
static inline int b64_val(uint8_t c) {
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}
static inline bool is_ascii_ws(uint8_t c) {
	// WHATWG "ASCII whitespace": tab, LF, FF, CR, space.
	return c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == 0x20;
}

// atob(): decode a base64 string into a "binary string" — each output char is a
// byte in 0..255 (Latin-1), NOT UTF-8. Using a one-byte (Latin-1) string is the
// whole point: a previous UTF-8 round-trip mangled any decoded byte >= 0x80
// (e.g. 0xFF), so atob() couldn't be used to recover raw bytes.
//
// Implements the WHATWG "forgiving-base64 decode" algorithm directly rather
// than mbedtls_base64_decode, which diverges from the spec (it returns ""/wrong
// for unpadded short input like "ab"/"abc", accepts "a" without throwing, and
// rejects internal ASCII whitespace that the spec strips).
void nx_atob(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	// Per spec the argument is stringified (ToString) first.
	Local<String> in_str;
	if (!info[0]->ToString(iso->GetCurrentContext()).ToLocal(&in_str)) {
		return; // pending exception from coercion
	}

	int input_len = in_str->Length();
	if (input_len == 0) {
		info.GetReturnValue().Set(nx_str(iso, ""));
		return;
	}
	// A code unit > 0xFF can't be a base64 char. Reject non-one-byte input up
	// front rather than letting WriteOneByte truncate it to a stray byte that
	// might masquerade as a valid base64 char (matches btoa's check).
	if (!in_str->ContainsOnlyOneByte()) {
		throw_invalid_character(
		    iso, "The string to be decoded is not correctly encoded.");
		return;
	}
	// Read the input as Latin-1 bytes (base64 is ASCII, one byte per char).
	uint8_t *raw = (uint8_t *)malloc((size_t)input_len);
	if (!raw) {
		nx_throw(iso, "out of memory");
		return;
	}
	in_str->WriteOneByte(iso, 0, input_len, raw);

	// 1. Strip ASCII whitespace.
	uint8_t *buf = (uint8_t *)malloc((size_t)input_len);
	if (!buf) {
		free(raw);
		nx_throw(iso, "out of memory");
		return;
	}
	int n = 0;
	for (int i = 0; i < input_len; i++) {
		if (!is_ascii_ws(raw[i]))
			buf[n++] = raw[i];
	}
	free(raw);

	auto fail = [&]() {
		free(buf);
		throw_invalid_character(
		    iso, "The string to be decoded is not correctly encoded.");
	};

	// 2. Remove at most two trailing '=' (and only if length % 4 allows it).
	if (n > 0 && buf[n - 1] == '=') {
		n--;
		if (n > 0 && buf[n - 1] == '=')
			n--;
	}
	// 3. length % 4 == 1 is never valid.
	if (n % 4 == 1) {
		fail();
		return;
	}
	// 4. Every remaining char must be in the base64 alphabet.
	for (int i = 0; i < n; i++) {
		if (b64_val(buf[i]) < 0) {
			fail();
			return;
		}
	}

	// 5. Decode 4 chars -> 3 bytes; trailing 2 chars -> 1 byte, 3 -> 2 bytes.
	size_t out_cap = ((size_t)n / 4) * 3 + 2;
	uint8_t *output = (uint8_t *)malloc(out_cap ? out_cap : 1);
	if (!output) {
		free(buf);
		nx_throw(iso, "out of memory");
		return;
	}
	size_t output_len = 0;
	int i = 0;
	while (i + 4 <= n) {
		int a = b64_val(buf[i]), b = b64_val(buf[i + 1]);
		int c = b64_val(buf[i + 2]), d = b64_val(buf[i + 3]);
		uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;
		output[output_len++] = (v >> 16) & 0xFF;
		output[output_len++] = (v >> 8) & 0xFF;
		output[output_len++] = v & 0xFF;
		i += 4;
	}
	int rem = n - i;
	if (rem == 2) {
		int a = b64_val(buf[i]), b = b64_val(buf[i + 1]);
		uint32_t v = (a << 18) | (b << 12);
		output[output_len++] = (v >> 16) & 0xFF;
	} else if (rem == 3) {
		int a = b64_val(buf[i]), b = b64_val(buf[i + 1]),
		    c = b64_val(buf[i + 2]);
		uint32_t v = (a << 18) | (b << 12) | (c << 6);
		output[output_len++] = (v >> 16) & 0xFF;
		output[output_len++] = (v >> 8) & 0xFF;
	}
	free(buf);

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
