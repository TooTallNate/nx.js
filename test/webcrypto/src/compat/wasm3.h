#pragma once
typedef struct M3Environment {} *IM3Environment;
static inline IM3Environment m3_NewEnvironment(void) { return (void*)0; }
static inline void m3_FreeEnvironment(IM3Environment env) { (void)env; }
