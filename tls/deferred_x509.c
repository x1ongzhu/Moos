#include "deferred_x509.h"
#include <string.h>

static void start_chain(const br_x509_class **opaque,const char *host)
{
 MossDeferredX509 *ctx=(MossDeferredX509 *)opaque;
 ctx->count=ctx->used=ctx->remaining=0; ctx->error=0; ctx->host=host;
 br_x509_decoder_init(&ctx->leaf,NULL,NULL);
}
static void start_cert(const br_x509_class **opaque,uint32_t length)
{
 MossDeferredX509 *ctx=(MossDeferredX509 *)opaque;
 if(ctx->error) return;
 if(ctx->count==8 || length>sizeof(ctx->certificates)-ctx->used) {
  ctx->error=BR_ERR_X509_LIMIT_EXCEEDED; return;
 }
 ctx->lengths[ctx->count++]=length; ctx->remaining=length;
}
static void append(const br_x509_class **opaque,const unsigned char *data,size_t length)
{
 MossDeferredX509 *ctx=(MossDeferredX509 *)opaque;
 if(ctx->error) return;
 if(length>ctx->remaining) { ctx->error=BR_ERR_X509_TRUNCATED; return; }
 memcpy(ctx->certificates+ctx->used,data,length);
 ctx->used+=length; ctx->remaining-=length;
 if(ctx->count==1) br_x509_decoder_push(&ctx->leaf,data,length);
}
static void end_cert(const br_x509_class **opaque)
{
 MossDeferredX509 *ctx=(MossDeferredX509 *)opaque;
 if(!ctx->error && ctx->remaining) ctx->error=BR_ERR_X509_TRUNCATED;
 if(!ctx->error && ctx->count==1) ctx->error=br_x509_decoder_last_error(&ctx->leaf);
}
static unsigned end_chain(const br_x509_class **opaque)
{
 MossDeferredX509 *ctx=(MossDeferredX509 *)opaque;
 if(!ctx->error && !ctx->count) ctx->error=BR_ERR_X509_EMPTY_CHAIN;
 /* This permits only handshake traffic. Core still blocks all application
  * operations until moss_x509_validate completes successfully. */
 return ctx->error;
}
static const br_x509_pkey *get_pkey(const br_x509_class *const *opaque,unsigned *usage)
{
 MossDeferredX509 *ctx=(MossDeferredX509 *)opaque;
 if(ctx->error) return NULL;
 if(usage) *usage=BR_KEYTYPE_KEYX;
 return br_x509_decoder_get_pkey(&ctx->leaf);
}
static const br_x509_class deferred_vtable={
 sizeof(MossDeferredX509),start_chain,start_cert,append,end_cert,end_chain,get_pkey
};
void moss_x509_init(MossDeferredX509 *ctx)
{
 memset(ctx,0,sizeof(*ctx)); ctx->vtable=&deferred_vtable;
}
unsigned moss_x509_validate(MossDeferredX509 *ctx,br_x509_minimal_context *validator)
{
 const br_x509_class **v=&validator->vtable;
 const br_x509_pkey *key; unsigned usage=0,result; size_t i,offset=0;
 unsigned char fingerprint[32],length[4]; br_sha256_context hash;
 if(ctx->error) return ctx->error;
 if(!ctx->count || !ctx->host || !*ctx->host) return BR_ERR_X509_EMPTY_CHAIN;
 ctx->cacheHit=0;
 br_sha256_init(&hash);
 br_sha256_update(&hash,ctx->host,strlen(ctx->host)+1);
 for(i=0;i<ctx->count;++i) {
  length[0]=(unsigned char)(ctx->lengths[i]>>24);
  length[1]=(unsigned char)(ctx->lengths[i]>>16);
  length[2]=(unsigned char)(ctx->lengths[i]>>8);
  length[3]=(unsigned char)ctx->lengths[i];
  br_sha256_update(&hash,length,sizeof(length));
  br_sha256_update(&hash,ctx->certificates+offset,ctx->lengths[i]);
  offset+=ctx->lengths[i];
 }
 br_sha256_out(&hash,fingerprint); offset=0;
 if(ctx->cache && ctx->cache->valid &&
    !memcmp(fingerprint,ctx->cache->fingerprint,sizeof(fingerprint)) &&
    (ctx->days>ctx->cache->notBeforeDays ||
     (ctx->days==ctx->cache->notBeforeDays && ctx->seconds>=ctx->cache->notBeforeSeconds)) &&
    (ctx->days<ctx->cache->notAfterDays ||
     (ctx->days==ctx->cache->notAfterDays && ctx->seconds<=ctx->cache->notAfterSeconds))) {
  ctx->cacheHit=1; return 0;
 }
 (*v)->start_chain(v,ctx->host);
 for(i=0;i<ctx->count;++i) {
  (*v)->start_cert(v,(uint32_t)ctx->lengths[i]);
  (*v)->append(v,ctx->certificates+offset,ctx->lengths[i]);
  (*v)->end_cert(v); offset+=ctx->lengths[i];
 }
 result=(*v)->end_chain(v);
 if(result) return result;
 key=(*v)->get_pkey((const br_x509_class *const *)v,&usage);
 if(!key || key->key_type!=BR_KEYTYPE_RSA) return BR_ERR_X509_WRONG_KEY_TYPE;
 if(!(usage&BR_KEYTYPE_KEYX)) return BR_ERR_X509_FORBIDDEN_KEY_USAGE;
 if(ctx->cache) {
  MTValidationCache *cache=ctx->cache;
  cache->valid=0; cache->notBeforeDays=cache->notBeforeSeconds=0;
  cache->notAfterDays=0xffffffffUL; cache->notAfterSeconds=86399;
  offset=0;
  for(i=0;i<ctx->count;++i) {
   br_x509_decoder_init(&ctx->leaf,NULL,NULL);
   br_x509_decoder_push(&ctx->leaf,ctx->certificates+offset,ctx->lengths[i]);
   offset+=ctx->lengths[i];
   if(br_x509_decoder_last_error(&ctx->leaf)) return 0;
   if(ctx->leaf.notbefore_days>cache->notBeforeDays ||
      (ctx->leaf.notbefore_days==cache->notBeforeDays && ctx->leaf.notbefore_seconds>cache->notBeforeSeconds)) {
    cache->notBeforeDays=ctx->leaf.notbefore_days; cache->notBeforeSeconds=ctx->leaf.notbefore_seconds;
   }
   if(ctx->leaf.notafter_days<cache->notAfterDays ||
      (ctx->leaf.notafter_days==cache->notAfterDays && ctx->leaf.notafter_seconds<cache->notAfterSeconds)) {
    cache->notAfterDays=ctx->leaf.notafter_days; cache->notAfterSeconds=ctx->leaf.notafter_seconds;
   }
  }
  memcpy(cache->fingerprint,fingerprint,sizeof(fingerprint)); cache->valid=1;
 }
 return 0;
}
