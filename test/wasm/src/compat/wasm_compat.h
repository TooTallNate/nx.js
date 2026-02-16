#pragma once
/**
 * Compatibility shim for wasm3 API differences.
 *
 * The nx.js wasm.c was written against a version of wasm3 where m3_Realloc
 * accepted 4 arguments: (label, ptr, newSize, oldSize). In wasm3 v0.5.0,
 * m3_Realloc takes 3 arguments: (ptr, newSize, oldSize) and returns void*.
 *
 * This header wraps the 4-arg call to drop the label and forward to the
 * 3-arg function.
 */

#include <m3_core.h>

// Override m3_Realloc to accept and ignore the label parameter.
// The real function in wasm3 v0.5.0: void* m3_Realloc(void*, size_t, size_t)
#ifdef m3_Realloc
#undef m3_Realloc
#endif

static inline void *m3_Realloc_compat(const char *label, void *ptr,
                                       size_t newSize, size_t oldSize) {
	(void)label;
	// Use the real function name from wasm3 v0.5.0 source
	extern void *m3_Realloc(void *i_ptr, size_t i_newSize, size_t i_oldSize);
	return m3_Realloc(ptr, newSize, oldSize);
}

// Redirect 4-arg m3_Realloc calls to our compat wrapper
#define m3_Realloc(LABEL, PTR, NEW, OLD) m3_Realloc_compat(LABEL, PTR, NEW, OLD)
