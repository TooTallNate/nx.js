#pragma once
// Stub ft2build.h for host builds — types.h includes this but compression.c
// doesn't need any FreeType functionality.
//
// Provide a minimal FT_Library typedef so types.h (nx_context_t) compiles.
typedef void *FT_Library;
