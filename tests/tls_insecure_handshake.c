#define main handshake_main
#include "tls_handshake.c"
#undef main

int main(int argc,char **argv)
{
 char *valid[4],*wrong[4],*expired[5];
 if(argc!=3) return 2;
 valid[0]=argv[0]; valid[1]=argv[1]; valid[2]=argv[2]; valid[3]=argv[1];
 if(handshake_main(4,valid) || !lastInsecure || lastWarmup) return 10;
 memcpy(wrong,valid,sizeof(wrong)); wrong[3]="invalid.invalid";
 if(handshake_main(4,wrong) || !lastInsecure) return 11;
 memcpy(expired,valid,sizeof(valid)); expired[4]="946684800";
 if(handshake_main(5,expired) || !lastInsecure) return 12;
 puts("Experimental build: TLS encryption works; hostname/time verification intentionally skipped");
 return 0;
}
