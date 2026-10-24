#include "bearssl.h"
#include <stdio.h>
#include <string.h>

extern uint32_t moss_rsa_verify_public(unsigned char *, size_t,
 const br_rsa_public_key *);

static size_t decode(unsigned char *out, const char *s)
{
 size_t i; unsigned value;
 for(i=0;s[2*i];++i) {
  if(sscanf(s+2*i,"%2x",&value)!=1) return 0;
  out[i]=(unsigned char)value;
 }
 return i;
}

int main(void)
{
 char nh[1025],xh[1025],eh[17],expectedh[1025];
 unsigned char n[512],x[512],original[512],e[8],expected[512];
 br_rsa_public_key pk; unsigned cases=0;
 while(scanf("%1024s %1024s %16s %1024s",nh,xh,eh,expectedh)==4) {
  pk.n=n; pk.nlen=decode(n,nh); pk.e=e; pk.elen=decode(e,eh);
  if(decode(original,xh)!=pk.nlen || decode(expected,expectedh)!=pk.nlen) return 2;
  memcpy(x,original,pk.nlen);
  if(!br_rsa_i15_public(x,pk.nlen,&pk) || memcmp(x,expected,pk.nlen)) return 3;
  memcpy(x,original,pk.nlen);
  if(!moss_rsa_verify_public(x,pk.nlen,&pk) || memcmp(x,expected,pk.nlen)) return 4;
  memcpy(x,n,pk.nlen);
  if(br_rsa_i15_public(x,pk.nlen,&pk)) return 5;
  memcpy(x,n,pk.nlen);
  if(moss_rsa_verify_public(x,pk.nlen,&pk)) return 6;
  ++cases;
 }
 printf("RSA: %u independent vectors passed on both arithmetic paths\n",cases);
 return cases?0:7;
}
