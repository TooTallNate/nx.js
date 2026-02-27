#pragma once
// Minimal base64 implementation for host builds (replaces mbedtls)
#include <stddef.h>
#include <string.h>

#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL -0x002A

static inline int mbedtls_base64_encode(unsigned char *dst, size_t dlen,
                                         size_t *olen, const unsigned char *src,
                                         size_t slen) {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t n = (slen / 3) * 4;
    if (slen % 3) n += 4;

    *olen = n;

    if (dst == NULL || dlen == 0) return 0;
    if (dlen < n) return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;

    unsigned char *p = dst;
    size_t i;
    for (i = 0; i + 2 < slen; i += 3) {
        *p++ = b64[(src[i] >> 2) & 0x3F];
        *p++ = b64[((src[i] & 0x3) << 4) | (src[i+1] >> 4)];
        *p++ = b64[((src[i+1] & 0xF) << 2) | (src[i+2] >> 6)];
        *p++ = b64[src[i+2] & 0x3F];
    }
    if (i < slen) {
        *p++ = b64[(src[i] >> 2) & 0x3F];
        if (i + 1 < slen) {
            *p++ = b64[((src[i] & 0x3) << 4) | (src[i+1] >> 4)];
            *p++ = b64[((src[i+1] & 0xF) << 2)];
        } else {
            *p++ = b64[((src[i] & 0x3) << 4)];
            *p++ = '=';
        }
        *p++ = '=';
    }

    return 0;
}
