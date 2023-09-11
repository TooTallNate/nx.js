#include "error.h"

void print_js_error(JSContext *ctx)
{
    JSValue exception_val = JS_GetException(ctx);
    const char *exception_str = JS_ToCString(ctx, exception_val);
    printf("%s\n", exception_str);
    JS_FreeCString(ctx, exception_str);

    JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
    const char *stack_str = JS_ToCString(ctx, stack_val);
    printf("%s\n", stack_str);
    JS_FreeCString(ctx, stack_str);

    JS_FreeValue(ctx, exception_val);
}
