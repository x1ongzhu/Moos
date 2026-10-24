#ifndef MOSS_CORE_ABI_H
#define MOSS_CORE_ABI_H
/* Every integer crossing the 16-bit Palm / 32-bit crypto ABI is explicitly long. */
typedef unsigned long MTWord;
typedef struct {
 unsigned char fingerprint[32];
 MTWord valid,notBeforeDays,notBeforeSeconds,notAfterDays,notAfterSeconds;
} MTValidationCache;
typedef struct {
 const char *host;
 unsigned char seed[32];
 MTWord days, seconds;
 MTValidationCache *cache;
} MTInit;
typedef struct { void *data; MTWord length; } MTBuffer;
#define MT_SIZE 0
#define MT_INIT 1
#define MT_STATE 2
#define MT_ERROR 3
#define MT_SENDREC 4
#define MT_SENDREC_ACK 5
#define MT_RECVREC 6
#define MT_RECVREC_ACK 7
#define MT_SENDAPP 8
#define MT_SENDAPP_ACK 9
#define MT_RECVAPP 10
#define MT_RECVAPP_ACK 11
#define MT_FLUSH 12
#define MT_CLOSE 13
#define MT_X509_ERROR 14
typedef MTWord (*MTCore)(MTWord op, void *context, void *arg);
#endif
