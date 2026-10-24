#define main handshake_main
#include "tls_handshake.c"
#undef main

int main(int argc,char **argv)
{
 char *valid[4],*wrong[4],*expired[5];
 if(argc!=3) return 2;
 valid[0]=argv[0]; valid[1]=argv[1]; valid[2]=argv[2]; valid[3]=argv[1];
 if(handshake_main(4,valid) || !lastWarmup || !validationCache.valid) return 10;
 if(handshake_main(4,valid) || lastWarmup) return 11;
 validationCache.fingerprint[0]^=1;
 if(handshake_main(4,valid) || !lastWarmup) return 14;
 memcpy(wrong,valid,sizeof(wrong)); wrong[3]="invalid.invalid";
 if(handshake_main(4,wrong)!=6 || lastX509!=56) return 12;
 memcpy(expired,valid,sizeof(valid)); expired[4]="946684800";
 if(handshake_main(5,expired)!=6 || lastX509!=54) return 13;
 puts("Certificate cache hit, fingerprint invalidation, hostname rejection and expiry rejection passed");
 return 0;
}
