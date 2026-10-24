#include "bearssl.h"
#include "core_abi.h"
#include "trust.h"
#include "deferred_x509.h"
#include <string.h>
#ifndef MOSS_NO_PREVERIFIED
#include "preverified_deepseek.h"
#endif
extern uint32_t moss_rsa_pkcs1_vrfy(const unsigned char *, size_t,
 const unsigned char *, size_t, const br_rsa_public_key *, unsigned char *);
/* Freestanding libc, all calls remain within the 32-bit crypto engine. */
#ifdef __palmos__
void *memcpy(void *d,const void *s,size_t n) { unsigned char *p=d; const unsigned char *q=s; while(n--) *p++=*q++; return d; }
void *memmove(void *d,const void *s,size_t n) { unsigned char *p=d; const unsigned char *q=s; if(p<q) while(n--) *p++=*q++; else { p+=n; q+=n; while(n--) *--p=*--q; } return d; }
void *memset(void *d,int c,size_t n) { unsigned char *p=d; while(n--) *p++=(unsigned char)c; return d; }
void bcopy(const void *s,void *d,size_t n) { memmove(d,s,n); }
int memcmp(const void *a,const void *b,size_t n) { const unsigned char *p=a,*q=b; while(n--) { if(*p!=*q) return *p-*q; ++p; ++q; } return 0; }
size_t strlen(const char *s) { size_t n=0; while(s[n]) ++n; return n; }
int strcmp(const char *a,const char *b) { while(*a && *a==*b) { ++a; ++b; } return (unsigned char)*a-(unsigned char)*b; }
#endif

br_prng_seeder br_prng_seeder_system(const char **name) { (void)name; return 0; }
typedef struct {
 br_ssl_client_context client;
 br_x509_minimal_context x509;
 MossDeferredX509 deferred;
 unsigned checked,validationError;
 unsigned char input[BR_SSL_BUFSIZE_INPUT];
 unsigned char output[1109];
} Core;
MTWord MossTLSCore(MTWord op,void *memory,void *arg) {
 Core *c=memory; MTBuffer *b=arg; size_t len;
 if(op!=MT_SIZE && op!=MT_INIT && c->validationError) {
  if(op==MT_STATE) return BR_SSL_CLOSED;
  if(op==MT_ERROR || op==MT_X509_ERROR) return c->validationError;
  if(op==MT_SENDAPP || op==MT_RECVAPP || op==MT_SENDREC || op==MT_RECVREC) {
   b->data=NULL; b->length=0;
  }
  return 0;
 }
 if(op!=MT_SIZE && op!=MT_INIT && !c->checked) {
  if(op==MT_SENDAPP || op==MT_RECVAPP) { b->data=NULL; b->length=0; return 0; }
  if(op==MT_SENDAPP_ACK || op==MT_RECVAPP_ACK || op==MT_FLUSH) return 0;
 }
 switch(op) {
 case MT_SIZE: return sizeof(Core);
 case MT_INIT: {
  MTInit *init=arg;
  static const uint16_t suites[]={BR_TLS_RSA_WITH_AES_128_GCM_SHA256};
  memset(c,0,sizeof(*c)); br_ssl_client_zero(&c->client);
  br_ssl_engine_add_flags(&c->client.eng,BR_OPT_NO_RENEGOTIATION);
  br_ssl_engine_set_versions(&c->client.eng,BR_TLS12,BR_TLS12);
  br_ssl_engine_set_suites(&c->client.eng,suites,1);
  br_ssl_engine_set_hash(&c->client.eng,br_sha256_ID,&br_sha256_vtable);
  br_ssl_engine_set_prf_sha256(&c->client.eng,br_tls12_sha256_prf);
  br_ssl_client_set_rsapub(&c->client,br_rsa_i15_public);
  br_ssl_engine_set_rsavrfy(&c->client.eng,moss_rsa_pkcs1_vrfy);
  br_ssl_engine_set_aes_ctr(&c->client.eng,&br_aes_ct_ctr_vtable);
  br_ssl_engine_set_ghash(&c->client.eng,br_ghash_ctmul32);
  br_ssl_engine_set_gcm(&c->client.eng,&br_sslrec_in_gcm_vtable,&br_sslrec_out_gcm_vtable);
  br_x509_minimal_init(&c->x509,&br_sha256_vtable,roots,sizeof(roots)/sizeof(roots[0]));
  br_x509_minimal_set_hash(&c->x509,br_sha256_ID,&br_sha256_vtable);
  br_x509_minimal_set_rsa(&c->x509,moss_rsa_pkcs1_vrfy);
  br_x509_minimal_set_time(&c->x509,init->days,init->seconds);
  moss_x509_init(&c->deferred);
#if !defined(MOSS_NO_PREVERIFIED) && MOSS_HAS_PREVERIFIED
  if(init->cache && !init->cache->valid && !strcmp(init->host,"api.deepseek.com")) {
   memcpy(init->cache,&moss_preverified_deepseek,sizeof(*init->cache));
  }
#endif
  c->deferred.cache=init->cache; c->deferred.days=init->days; c->deferred.seconds=init->seconds;
  br_ssl_engine_set_x509(&c->client.eng,&c->deferred.vtable);
  br_ssl_engine_set_buffers_bidi(&c->client.eng,c->input,sizeof(c->input),c->output,sizeof(c->output));
  br_ssl_engine_inject_entropy(&c->client.eng,init->seed,32);
  if(!br_ssl_client_reset(&c->client,init->host,0)) return 0;
  br_hmac_drbg_generate(&c->client.eng.rng,init->seed,32);
  return 1;
 }
 case MT_STATE: {
  unsigned state=br_ssl_engine_current_state(&c->client.eng);
  if((state&BR_SSL_SENDAPP) && !c->checked) {
#if MOSS_TLS_INSECURE
   c->validationError=0;
#else
   c->validationError=moss_x509_validate(&c->deferred,&c->x509);
#endif
   c->checked=1;
   if(c->validationError) return BR_SSL_CLOSED;
  }
#if MOSS_TLS_INSECURE
  state|=64;
#else
  if((state&BR_SSL_SENDAPP) && c->deferred.cache && !c->deferred.cacheHit) state|=32;
#endif
  return state;
 }
 case MT_ERROR: return br_ssl_engine_last_error(&c->client.eng);
 case MT_X509_ERROR:
#if MOSS_TLS_INSECURE
  return c->deferred.error;
#else
  return c->checked?BR_ERR_X509_OK:(c->deferred.error?c->deferred.error:c->x509.err);
#endif
 case MT_SENDREC: b->data=br_ssl_engine_sendrec_buf(&c->client.eng,&len); b->length=len; return len;
 case MT_SENDREC_ACK: br_ssl_engine_sendrec_ack(&c->client.eng,*(MTWord *)arg); return 0;
 case MT_RECVREC: b->data=br_ssl_engine_recvrec_buf(&c->client.eng,&len); b->length=len; return len;
 case MT_RECVREC_ACK: br_ssl_engine_recvrec_ack(&c->client.eng,*(MTWord *)arg); return 0;
 case MT_SENDAPP: b->data=br_ssl_engine_sendapp_buf(&c->client.eng,&len); b->length=len; return len;
 case MT_SENDAPP_ACK: br_ssl_engine_sendapp_ack(&c->client.eng,*(MTWord *)arg); return 0;
 case MT_RECVAPP: b->data=br_ssl_engine_recvapp_buf(&c->client.eng,&len); b->length=len; return len;
 case MT_RECVAPP_ACK: br_ssl_engine_recvapp_ack(&c->client.eng,*(MTWord *)arg); return 0;
 case MT_FLUSH: br_ssl_engine_flush(&c->client.eng,0); return 0;
 case MT_CLOSE: br_ssl_engine_close(&c->client.eng); return 0;
 }
 return 0;
}
