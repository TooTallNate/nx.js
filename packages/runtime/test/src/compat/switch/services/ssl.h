#pragma once
/**
 * Stub for <switch/services/ssl.h> used by tls.cc.
 *
 * The real libnx ssl.h provides functions to load system CA certificates from
 * the Switch SSL service. On the host, CA certs are loaded from system files
 * and nx_context.ca_certs_loaded is set true before nx_tls_load_ca_certs()
 * runs, so these are never exercised — they only need to link with matching
 * signatures and fail cleanly.
 */

#include <stdint.h>

typedef uint32_t Result;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;

typedef enum {
	SslTrustedCertStatus_EnabledTrusted = 2,
} SslTrustedCertStatus;

// tls.cc indexes a SslBuiltInCertificateInfo array out of the cert buffer and
// reads .status / .cert_data / .cert_size. Match the libnx layout.
typedef struct {
	u32 cert_id;
	u32 status;
	u64 cert_size;
	u8 *cert_data;
} SslBuiltInCertificateInfo;

static inline Result sslInitialize(u32 num_sessions) {
	(void)num_sessions;
	return 1; // R_FAILED — not available on host (CA certs loaded from files)
}

static inline void sslExit(void) {}

static inline Result sslGetCertificateBufSize(u32 *ca_cert_ids, u32 count,
                                              u32 *out) {
	(void)ca_cert_ids;
	(void)count;
	(void)out;
	return 1; // R_FAILED — host loads CA certs from files instead
}

static inline Result sslGetCertificates(void *buffer, u32 size,
                                        u32 *ca_cert_ids, u32 count,
                                        u32 *total_out) {
	(void)buffer;
	(void)size;
	(void)ca_cert_ids;
	(void)count;
	(void)total_out;
	return 1; // R_FAILED
}
