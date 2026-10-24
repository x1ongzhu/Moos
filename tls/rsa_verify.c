/* Certificate signatures and RSA public keys contain no secret operands.
 * Keep their unpadded hardware multiplication isolated from key exchange. */
#define br_rsa_i15_public moss_rsa_verify_public
#define br_rsa_i15_pkcs1_vrfy moss_rsa_pkcs1_vrfy
#define br_i15_modpow_opt moss_i15_verify_modpow
#define br_i15_montymul moss_i15_verify_montymul
#define MOSS_TLS_PUBLIC_ARITH 1
#include "inner.h"

#ifdef __palmos__
static inline uint32_t moss_verify_mul15(uint32_t x, uint32_t y)
{
 uint32_t result;
 __asm__("mulu.w %2,%0" : "=&d"(result) : "0"(x), "d"(y) : "cc");
 return result;
}
#undef MUL15
#define MUL15(x,y) moss_verify_mul15((uint32_t)(x),(uint32_t)(y))
#endif

#include "../vendor/bearssl-0.6/src/int/i15_montmul.c"
#include "../vendor/bearssl-0.6/src/int/i15_modpow2.c"
#include "../vendor/bearssl-0.6/src/rsa/rsa_i15_pub.c"
#include "../vendor/bearssl-0.6/src/rsa/rsa_i15_pkcs1_vrfy.c"
