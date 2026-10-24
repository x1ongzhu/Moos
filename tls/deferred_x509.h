#ifndef MOSS_DEFERRED_X509_H
#define MOSS_DEFERRED_X509_H
#include "bearssl.h"
#include "core_abi.h"
/* Bound untrusted certificate storage before any application data is allowed. */
typedef struct {
 const br_x509_class *vtable;
 br_x509_decoder_context leaf;
 unsigned char certificates[6144];
 size_t lengths[8],count,used,remaining;
 unsigned error;
 const char *host;
 MTValidationCache *cache;
 MTWord days,seconds;
 unsigned cacheHit;
} MossDeferredX509;
void moss_x509_init(MossDeferredX509 *ctx);
unsigned moss_x509_validate(MossDeferredX509 *ctx,br_x509_minimal_context *validator);
#endif
