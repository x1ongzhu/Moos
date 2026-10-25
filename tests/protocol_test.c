#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/protocol.h"
static void urls(void) {
 MossURL u;
 assert(moss_url("http://localhost:8080/v1/",&u));
 assert(!strcmp(u.host,"localhost") && !strcmp(u.path,"/v1") && u.port==8080 && !u.tls);
 assert(moss_url("https://example.com/prefix/v1",&u) && u.tls && u.port==443);
 assert(!moss_url("",&u)); assert(!moss_url("h",&u));
 assert(!moss_url("http://x:65536/v1",&u)); assert(!moss_url("http://x:0/v1",&u));
 assert(!moss_url("http://user@x/v1",&u)); assert(!moss_url("http://x/\r\nInjected",&u));
 assert(!moss_url("http://x/v1?key=secret",&u));
}
static void requests(void) {
 MossURL u; char request[MOSS_REQUEST_MAX],small[16],*body,*length;
 MossMessage m[2]={{"user","quote \" and \\ and\nnewline"},{"assistant","answer"}};
 int n; unsigned expected;
 assert(moss_url("http://x:8080/v1/",&u));
 n=moss_request(request,sizeof(request),&u,"secret","model",m,2);
 assert(n>0 && (unsigned)n==strlen(request));
 assert(strstr(request,"POST /v1/chat/completions HTTP/1.1\r\n"));
 assert(strstr(request,"Authorization: Bearer secret\r\n"));
 assert(strstr(request,"\\\" and \\\\ and\\u000anewline"));
 body=strstr(request,"\r\n\r\n")+4; length=strstr(request,"Content-Length: ");
 assert(sscanf(length,"Content-Length: %u",&expected)==1 && expected==strlen(body));
 assert(strstr(body,"\"stream\":true"));
 assert(strstr(request,"Accept: text/plain\r\n"));
 assert(strstr(request,"Connection: keep-alive\r\n"));
 assert(moss_request(small,sizeof(small),&u,"","m",m,2)<0);
 assert(moss_request(request,sizeof(request),&u,"bad\r\nKey","m",m,2)<0);
}
static void framing(void) {
 char wire[MOSS_HTTP_MAX+1]; int status, n; unsigned i;
 const char *chunked="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4;foo=bar\r\nWiki\r\n5\r\npedia\r\n0\r\nX-Test: yes\r\n\r\n";
 strcpy(wire,"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello");
 n=(int)strlen(wire);
 for(i=0;i<(unsigned)n;++i) assert(moss_http_complete(wire,i,0)==0);
 assert(moss_http_complete(wire,n,0)==1);
 assert(moss_http_complete(wire,n-1,1)==-1);
 assert(moss_http_body(wire,n,&status)==5 && status==200 && !strcmp(wire,"Hello"));
 strcpy(wire,chunked); n=(int)strlen(wire);
 for(i=0;i<(unsigned)n;++i) assert(moss_http_complete(wire,i,0)==0);
 assert(moss_http_complete(wire,n,0)==1);
 assert(moss_http_body(wire,n,&status)==9 && !strcmp(wire,"Wikipedia"));
 strcpy(wire,"HTTP/1.0 401 Nope\r\n\r\n{}"); n=(int)strlen(wire);
 assert(moss_http_complete(wire,n,0)==0 && moss_http_complete(wire,n,1)==1);
 assert(moss_http_body(wire,n,&status)==2 && status==401);
 strcpy(wire,"HTTP/1.1 200 OK\r\nContent-Length: 4\r\nTransfer-Encoding: chunked\r\n\r\n");
 assert(moss_http_complete(wire,strlen(wire),0)==-1);
 strcpy(wire,"HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n");
 assert(moss_http_complete(wire,strlen(wire),0)==-1);
 strcpy(wire,"HTTP/1.1 200 OK\r\nContent-Length: 999999999\r\n\r\n");
 assert(moss_http_complete(wire,strlen(wire),0)==-1);
 strcpy(wire,"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nOKxx");
 assert(moss_http_complete(wire,strlen(wire),0)==-1);
}
static void persistence(void) {
 const char *cases[]={
  "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK",
  "HTTP/1.1 200 OK\r\nConnection: upgrade, ClOsE\r\nContent-Length: 2\r\n\r\nOK",
  "HTTP/1.0 200 OK\r\nContent-Length: 2\r\n\r\nOK",
  "HTTP/1.0 200 OK\r\nConnection: Keep-Alive\r\nContent-Length: 2\r\n\r\nOK",
  "HTTP/1.1 200 OK\r\nConnection: close\r\nConnection: keep-alive\r\nContent-Length: 2\r\n\r\nOK",
  "HTTP/1.1 200 OK\r\n\r\nOK",
  "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOKEXTRA",
  "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nO",
  "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nOK\r\n0\r\n\r\n",
  "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nOK\r\n0\r\nX-Test: yes\r\n\r\n",
  "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nOK\r\n0\r\n\r\nEXTRA"
 };
 int expected[]={1,0,0,1,0,0,0,0,1,1,0}; unsigned i;
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
  assert(moss_http_reusable(cases[i],strlen(cases[i]))==expected[i]);
}
static void display_text(void) {
 char sample[]="It\xe2\x80\x99s \xe2\x80\x9cgood\xe2\x80\x9d\xe2\x80\x94really\xe2\x80\xa6";
 char plain[]="It's \"good\" - really...";
 char other[]="\xe4\xbd\xa0\xe5\xa5\xbd \xf0\x9f\x98\x80";
 char spaces[]="a\xc2\xa0" "b\xe2\x80\xaf" "c\xe2\x80\xa2";
 char broken[]="end\xe2\x80";
 moss_display_ascii(sample); assert(!strcmp(sample,"It's \"good\"-really..."));
 moss_display_ascii(plain); assert(!strcmp(plain,"It's \"good\" - really..."));
 moss_display_ascii(other); assert(!strcmp(other,"?? ?"));
 moss_display_ascii(spaces); assert(!strcmp(spaces,"a b c*"));
 moss_display_ascii(broken); assert(!strcmp(broken,"end?"));
}
int main(void) { urls(); requests(); framing(); persistence(); display_text(); puts("Protocol checks passed"); return 0; }
