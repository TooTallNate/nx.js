/**
 * Runtime Test Host
 *
 * Minimal QuickJS host for testing the nx.js runtime polyfills (URL,
 * EventTarget, TextEncoder, FormData, etc.) on x86_64 without a Switch.
 *
 * Usage: nxjs-runtime-test <runtime.js> <test.js>
 *   Exit code 0 = all tests passed, non-zero = failure.
 */

#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* nx_init_url from url_copy.c */
void nx_init_url(JSContext *ctx, JSValueConst this_val);

static char *read_file(const char *path, size_t *out_len) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Cannot open file: %s\n", path);
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc(len + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t rd = fread(buf, 1, len, f);
	fclose(f);
	buf[rd] = '\0';
	if (out_len) *out_len = rd;
	return buf;
}

static void print_js_error(JSContext *ctx) {
	JSValue exc = JS_GetException(ctx);
	const char *str = JS_ToCString(ctx, exc);
	if (str) { fprintf(stderr, "%s\n", str); JS_FreeCString(ctx, str); }
	JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
	if (!JS_IsUndefined(stack)) {
		const char *s = JS_ToCString(ctx, stack);
		if (s) { fprintf(stderr, "%s\n", s); JS_FreeCString(ctx, s); }
	}
	JS_FreeValue(ctx, stack);
	JS_FreeValue(ctx, exc);
}

static int eval_file(JSContext *ctx, const char *path, const char *label) {
	size_t len;
	char *src = read_file(path, &len);
	if (!src) { fprintf(stderr, "Failed to read %s: %s\n", label, path); return -1; }
	JSValue val = JS_Eval(ctx, src, len, path, JS_EVAL_TYPE_GLOBAL);
	free(src);
	if (JS_IsException(val)) {
		fprintf(stderr, "%s evaluation failed:\n", label);
		print_js_error(ctx);
		JS_FreeValue(ctx, val);
		return -1;
	}
	JS_FreeValue(ctx, val);
	return 0;
}

static void drain_jobs(JSContext *ctx) {
	JSContext *ctx1;
	for (;;) {
		int r = JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx1);
		if (r <= 0) {
			if (r < 0) print_js_error(ctx1);
			break;
		}
	}
}

static JSValue js_drain_microtasks(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	(void)this_val; (void)argc; (void)argv;
	drain_jobs(ctx);
	return JS_UNDEFINED;
}

static int g_exit_code = 0;
static JSValue js_exit(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc > 0) JS_ToInt32(ctx, &g_exit_code, argv[0]);
	return JS_UNDEFINED;
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc > 0) {
		const char *s = JS_ToCString(ctx, argv[0]);
		if (s) { fputs(s, stdout); fflush(stdout); JS_FreeCString(ctx, s); }
	}
	return JS_UNDEFINED;
}

static JSValue js_print_err(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
	(void)this_val;
	if (argc > 0) {
		const char *s = JS_ToCString(ctx, argv[0]);
		if (s) { fputs(s, stderr); fflush(stderr); JS_FreeCString(ctx, s); }
	}
	return JS_UNDEFINED;
}

static const char *BOOTSTRAP_JS =
	"$.version = { nxjs: '0.0.0-test', hos: '0.0.0' };\n"
	"$.entrypoint = 'file:///test.js';\n"
	"$.argv = [];\n"
	"globalThis.$ = new Proxy($, {\n"
	"    get: function(target, prop) {\n"
	"        if (prop in target) return target[prop];\n"
	"        return function() { return {}; };\n"
	"    }\n"
	"});\n";

/**
 * Post-runtime JS: set up process object for uvu test runner.
 * NOTE: runtime.js does `delete globalThis.$` so we must NOT reference $
 * here. Use console.print (defined by runtime) for stdout output.
 */
static const char *POST_RUNTIME_JS =
	/* Override setTimeout to fire callbacks synchronously via microtask queue */
	"globalThis.setTimeout = function(fn, ms) {\n"
	"    if (typeof fn === 'function') {\n"
	"        Promise.resolve().then(function() { fn(); });\n"
	"    }\n"
	"    return 0;\n"
	"};\n"
	/* Provide process object for uvu */
	"globalThis.process = {\n"
	"    exit: function(code) { __exit(code || 0); },\n"
	"    env: {},\n"
	"    argv: [],\n"
	"    stdout: {\n"
	"        write: function(s) { console.print(s); },\n"
	"        isTTY: false\n"
	"    },\n"
	"    on: function() {},\n"
	"    hrtime: function(prev) {\n"
	"        var now = Date.now();\n"
	"        if (prev) return [0, (now * 1e6) - (prev[0] * 1e9 + prev[1])];\n"
	"        return [Math.floor(now / 1000), (now % 1000) * 1e6];\n"
	"    }\n"
	"};\n"
	/* Make process.exitCode a setter that calls __exit */
	"var __exitCode = 0;\n"
	"Object.defineProperty(process, 'exitCode', {\n"
	"    set: function(v) { __exitCode = v || 0; __exit(__exitCode); },\n"
	"    get: function() { return __exitCode; },\n"
	"    configurable: true\n"
	"});\n";

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <runtime.js> <test.js>\n", argv[0]);
		return 1;
	}

	const char *runtime_path = argv[1];
	const char *test_path = argv[2];

	JSRuntime *rt = JS_NewRuntime();
	if (!rt) { fprintf(stderr, "Failed to create JS runtime\n"); return 1; }

	JSContext *ctx = JS_NewContext(rt);
	if (!ctx) { JS_FreeRuntime(rt); return 1; }

	JSValue global = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global, "__drainMicrotasks",
		JS_NewCFunction(ctx, js_drain_microtasks, "__drainMicrotasks", 0));
	JS_SetPropertyStr(ctx, global, "__exit",
		JS_NewCFunction(ctx, js_exit, "__exit", 1));

	{
		JSValue dollar = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, dollar, "print",
			JS_NewCFunction(ctx, js_print, "print", 1));
		JS_SetPropertyStr(ctx, dollar, "printErr",
			JS_NewCFunction(ctx, js_print_err, "printErr", 1));
		nx_init_url(ctx, dollar);
		JS_SetPropertyStr(ctx, global, "$", dollar);
	}
	JS_FreeValue(ctx, global);

	/* Bootstrap $ */
	JSValue bs = JS_Eval(ctx, BOOTSTRAP_JS, strlen(BOOTSTRAP_JS), "<bootstrap>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(bs)) { print_js_error(ctx); JS_FreeValue(ctx, bs); JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1; }
	JS_FreeValue(ctx, bs);

	/* Load runtime.js */
	if (eval_file(ctx, runtime_path, "runtime") != 0) {
		JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1;
	}
	drain_jobs(ctx);

	/* Post-runtime patching (after runtime.js deletes globalThis.$) */
	JSValue pr = JS_Eval(ctx, POST_RUNTIME_JS, strlen(POST_RUNTIME_JS), "<post-runtime>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(pr)) {
		fprintf(stderr, "post-runtime patching failed:\n");
		print_js_error(ctx);
		JS_FreeValue(ctx, pr);
	}
	JS_FreeValue(ctx, pr);

	/* Load and evaluate the test file */
	if (eval_file(ctx, test_path, "test") != 0) {
		JS_FreeContext(ctx); JS_FreeRuntime(rt); return 1;
	}

	/* Drain any remaining async work (promises from tests) */
	drain_jobs(ctx);

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return g_exit_code;
}
