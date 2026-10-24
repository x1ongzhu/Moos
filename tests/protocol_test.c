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
 assert(strstr(body,"\"stream\":false"));
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
static void replies(void) {
 char out[256],small[4]; int truncated;
 const char *json="{\"usage\":{\"nested\":[1,true,null]},\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Hi\\n\\u4f60\\u597d \\ud83d\\ude00\"}}]}";
 assert(moss_reply(json,out,sizeof(out),&truncated)>0);
 assert(!strcmp(out,"Hi\n\xe4\xbd\xa0\xe5\xa5\xbd \xf0\x9f\x98\x80") && !truncated);
 assert(moss_reply(json,small,sizeof(small),&truncated)==3 && truncated && !strcmp(small,"Hi\n"));
 assert(moss_reply("{\"choices\":[{\"message\":{\"content\":null}}]}",out,sizeof(out),&truncated)<0);
 assert(moss_reply("{\"choices\":[{\"message\":{\"content\":\"\\ud800\"}}]}",out,sizeof(out),&truncated)<0);
 assert(moss_reply("{\"choices\":[{\"message\":{\"content\":\"\\u0000\"}}]}",out,sizeof(out),&truncated)<0);
 assert(moss_reply("{\"choices\":[{\"message\":{\"content\":\"ok\"}}]}garbage",out,sizeof(out),&truncated)<0);
 assert(moss_reply("{\"choices\":[{\"message\":{\"content\":\"ok\"}}]",out,sizeof(out),&truncated)<0);
 assert(moss_error("{\"error\":{\"message\":\"Bad key\"}}",out,sizeof(out))==7 && !strcmp(out,"Bad key"));
}
int main(void) { urls(); requests(); framing(); replies(); puts("Protocol checks passed"); return 0; }
