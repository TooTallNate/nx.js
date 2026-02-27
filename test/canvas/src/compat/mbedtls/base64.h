#pragma once
// Stub mbedtls/base64.h for host builds
#include <stddef.h>
#include <string.h>

// Minimal base64 encode for canvas test harness
// Signature matches mbedtls_base64_encode
static inline int mbedtls_base64_encode(
    unsigned char *dst, size_t dlen, size_t *olen,
    const unsigned char *src, size_t slen)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t needed = ((slen + 2) / 3) * 4;
    *olen = needed;

    if (dst == NULL || dlen == 0)
        return 0; // length query

    if (dlen < needed)
        return -0x002A; // MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL

    size_t i = 0, j = 0;
    for (; i + 2 < slen; i += 3) {
        unsigned int v = ((unsigned int)src[i] << 16) |
                         ((unsigned int)src[i+1] << 8) |
                         src[i+2];
        dst[j++] = table[(v >> 18) & 0x3F];
        dst[j++] = table[(v >> 12) & 0x3F];
        dst[j++] = table[(v >> 6) & 0x3F];
        dst[j++] = table[v & 0x3F];
    }
    if (i < slen) {
        unsigned int v = (unsigned int)src[i] << 16;
        if (i + 1 < slen) v |= (unsigned int)src[i+1] << 8;
        dst[j++] = table[(v >> 18) & 0x3F];
        dst[j++] = table[(v >> 12) & 0x3F];
        dst[j++] = (i + 1 < slen) ? table[(v >> 6) & 0x3F] : '=';
        dst[j++] = '=';
    }
    return 0;
}
