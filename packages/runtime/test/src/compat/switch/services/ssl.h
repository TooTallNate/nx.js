#pragma once
/**
 * Stub for <switch/services/ssl.h> used by tls.c.
 *
 * The real libnx ssl.h provides functions to load system CA certificates
 * from the Switch SSL service. On the host, we stub these out — TLS
 * connections will use mbedtls defaults or user-supplied certificates.
 */

#include <stdint.h>

typedef uint32_t Result;
typedef uint32_t u32;
typedef uint64_t u64;

typedef enum {
	SslTrustedCertStatus_EnabledTrusted = 0,
} SslTrustedCertStatus;

typedef enum {
	SslCaCertificateId_All = -1,
} SslCaCertificateId;

typedef struct {
	SslCaCertificateId cert_id;
	SslTrustedCertStatus status;
	u64 cert_size;
	void *cert_data;
} SslBuiltInCertificateInfo;

static inline Result sslInitialize(u32 num_sessions) {
	(void)num_sessions;
	return 1; // R_FAILED — not available on host
}

static inline void sslExit(void) {}

static inline Result sslGetCertificateBufSize(SslCaCertificateId *ids,
                                              u32 count, u32 *out_size) {
	(void)ids;
	(void)count;
	(void)out_size;
	return 1; // R_FAILED
}

static inline Result sslGetCertificates(void *buffer, u32 buffer_size,
                                        SslCaCertificateId *ids, u32 count,
                                        u32 *out_count) {
	(void)buffer;
	(void)buffer_size;
	(void)ids;
	(void)count;
	(void)out_count;
	return 1; // R_FAILED
}
