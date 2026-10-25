#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/protocol.h"
static const char *head="HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nTransfer-Encoding: chunked\r\n\r\n";
static const char *text="Hello\n\xe4\xbd\xa0\xe5\xa5\xbd \xf0\x9f\x98\x80!\nLiteral data: [DONE] stays text.";
static const char *ending="0\r\nX-Test: yes\r\n\r\n";
static char *framed(const char *body,unsigned piece) {
 size_t len=strlen(body),i,n,used; char *wire=malloc(len*32+strlen(head)+128);
 assert(wire); strcpy(wire,head); used=strlen(wire);
 for(i=0;i<len;i+=n) {
  n=len-i; if(n>piece) n=piece;
  used+=(size_t)snprintf(wire+used,32,"%x;extension=yes\r\n",(unsigned)n);
  memcpy(wire+used,body+i,n); used+=n; memcpy(wire+used,"\r\n",2); used+=2;
 }
 strcpy(wire+used,ending); return wire;
}
static void split_checks(void) {
 MossStream *s=malloc(sizeof(*s)); char *wire=framed(text,1); unsigned n=(unsigned)strlen(wire),i,j; int r;
 assert(sizeof(*s)<6000);
 for(i=0;i<=n;++i) {
  moss_stream_init(s); assert(moss_stream_feed(s,wire,i,0)>=0);
  assert(moss_stream_feed(s,wire+i,n-i,0)==1);
  assert(!strcmp(s->answer,text) && s->reusable && !s->truncated);
 }
 for(j=1;j<=37;++j) {
  moss_stream_init(s); r=0;
  for(i=0;i<n;i+=j) { unsigned size=n-i; if(size>j) size=j; r=moss_stream_feed(s,wire+i,size,0); assert(r>=0); }
  assert(r==1);
 }
 moss_stream_init(s);
 for(i=0;i<n && !s->used;++i) assert(moss_stream_feed(s,wire+i,1,0)==0);
 assert(s->used && !s->complete);
 assert(moss_stream_feed(s,wire+i,n-i,0)==1);
 assert(moss_stream_feed(s,"X",1,0)==-1 && !s->reusable);
 free(wire); free(s);
}
static void rejection_checks(void) {
 MossStream *s=malloc(sizeof(*s)); char *wire;
 const char *bad[]={"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe4\xbd", "\x80"}; unsigned i;
 for(i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
  wire=framed(bad[i],1); moss_stream_init(s);
  assert(moss_stream_feed(s,wire,strlen(wire),0)==-1 && !s->reusable && s->error[0]); free(wire);
 }
 wire=framed(text,3); moss_stream_init(s);
 assert(moss_stream_feed(s,wire,strlen(wire)-strlen(ending),0)==0 && s->used);
 assert(moss_stream_feed(s,"",0,1)==-1 && !s->complete);
 free(wire);
 {
  const char *invalid="HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\n";
  moss_stream_init(s); assert(moss_stream_feed(s,invalid,strlen(invalid),0)==-1);
  invalid="HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nContent-Length: 2\r\n\r\nOK";
  moss_stream_init(s); assert(moss_stream_feed(s,invalid,strlen(invalid),0)==-1);
  invalid="HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\npartial";
  moss_stream_init(s); assert(moss_stream_feed(s,invalid,strlen(invalid),1)==-1);
 }
 free(s);
}
static void long_stream(void) {
 MossStream *s=malloc(sizeof(*s)); char letters[1001],*wire; unsigned i;
 memset(letters,'x',1000); letters[1000]=0;
 moss_stream_init(s); assert(moss_stream_feed(s,head,strlen(head),0)==0);
 for(i=0;i<8;++i) {
  wire=framed(letters,1);
  assert(moss_stream_feed(s,wire+strlen(head),strlen(wire)-strlen(head)-strlen(ending),0)==0);
  free(wire);
 }
 assert(moss_stream_feed(s,ending,strlen(ending),0)==1);
 assert(s->used==MOSS_REPLY_MAX && s->truncated && s->reusable);
 assert(s->total>MOSS_HTTP_MAX);
 for(i=0;i<s->used;++i) assert(s->answer[i]=='x');
 free(s);
}
static void lengths_and_errors(void) {
 MossStream *s=malloc(sizeof(*s)); char wire[512];
 const char *response="HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK";
 moss_stream_init(s); assert(moss_stream_feed(s,response,strlen(response),0)==1 && !strcmp(s->answer,"OK") && s->reusable);
 strcpy(wire,"HTTP/1.1 401 Nope\r\nContent-Type: text/plain\r\nContent-Length: 7\r\n\r\nbad key");
 moss_stream_init(s); assert(moss_stream_feed(s,wire,strlen(wire)-3,0)==0);
 assert(moss_stream_feed(s,wire+strlen(wire)-3,3,0)==-1 && s->status==401 && !strcmp(s->error,"bad key"));
 free(s);
}
int main(void) {
 split_checks(); rejection_checks(); long_stream(); lengths_and_errors();
 printf("Incremental HTTP text checks passed; state size %u bytes\n",(unsigned)sizeof(MossStream)); return 0;
}
