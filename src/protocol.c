#include "protocol.h"

static unsigned slen(const char *s) { unsigned n=0; while (s[n]) ++n; return n; }
static int equal(const char *s, const char *t, unsigned n) {
    while(n--) if(*s++ != *t++) return 0;
    return 1;
}
static char lower(char c) { return c>='A' && c<='Z' ? (char)(c+32) : c; }
static int iequal(const char *s, const char *t, unsigned n) {
    while(n--) if(lower(*s++) != lower(*t++)) return 0;
    return 1;
}
static int copy(char *out, unsigned cap, const char *s, unsigned n) {
    unsigned i; if(n>=cap) return 0;
    for(i=0;i<n;++i) out[i]=s[i]; out[n]=0; return 1;
}
int moss_url(const char *base, MossURL *u) {
    const char *p=base, *start; unsigned n=0; unsigned long port=0;
    if(slen(p)>=7 && equal(p,"http://",7)) { p+=7; u->tls=0; u->port=80; }
    else if(slen(p)>=8 && equal(p,"https://",8)) { p+=8; u->tls=1; u->port=443; }
    else return 0;
    start=p;
    while(*p && *p!='/' && *p!=':') {
        if((unsigned char)*p<=32 || *p=='?' || *p=='#' || *p=='@' || *p=='[') return 0;
        ++p; ++n;
    }
    if(!n || !copy(u->host,sizeof(u->host),start,n)) return 0;
    if(*p==':') {
        ++p; if(*p<'0' || *p>'9') return 0;
        while(*p>='0' && *p<='9') { if(port>6553) return 0; port=port*10+(*p++-'0'); }
        if(!port || port>65535) return 0; u->port=(unsigned short)port;
    }
    start=p; n=0;
    while(*p) { if((unsigned char)*p<=32 || *p=='?' || *p=='#') return 0; ++p; ++n; }
    while(n && start[n-1]=='/') --n;
    return copy(u->path,sizeof(u->path),start,n);
}
typedef struct { char *out; unsigned cap, n; int ok; } Writer;
static void put(Writer *w, char c) {
    if(w->n+1>=w->cap) { w->ok=0; return; }
    w->out[w->n++]=c; w->out[w->n]=0;
}
static void str(Writer *w,const char *s) { while(*s) put(w,*s++); }
static void number(Writer *w, unsigned n) {
    char buf[12]; unsigned i=0;
    do { buf[i++]=(char)('0'+n%10); n/=10; } while(n);
    while(i) put(w,buf[--i]);
}
static void quote(Writer *w,const char *s) {
    static const char hex[]="0123456789abcdef"; unsigned char c;
    put(w,'"');
    while(*s) {
        c=(unsigned char)*s++;
        if(c=='"' || c=='\\') { put(w,'\\'); put(w,(char)c); }
        else if(c<32) { str(w,"\\u00"); put(w,hex[c>>4]); put(w,hex[c&15]); }
        else put(w,(char)c);
    }
    put(w,'"');
}
static void body(Writer *w, const char *model, const MossMessage *m,unsigned count) {
    unsigned i;
    str(w,"{\"model\":"); quote(w,model); str(w,",\"stream\":false,\"messages\":[");
    for(i=0;i<count;++i) {
        if(i) put(w,','); str(w,"{\"role\":"); quote(w,m[i].role);
        str(w,",\"content\":"); quote(w,m[i].text); put(w,'}');
    }
    str(w,"]}");
}
int moss_request(char *out,unsigned cap,const MossURL *u,const char *key,
                 const char *model,const MossMessage *m,unsigned count) {
    Writer w; unsigned contentLen, i;
    if(!cap || !*model || !count) return -1;
    /* Only ASCII input is supported by the initial Palm UI. Header injection is rejected. */
    for(i=0;key[i];++i) if((unsigned char)key[i]<33 || (unsigned char)key[i]>126) return -1;
    w.out=out; w.cap=cap; w.n=0; w.ok=1; out[0]=0;
    body(&w,model,m,count); if(!w.ok) return -1; contentLen=w.n;
    w.n=0; out[0]=0;
    str(&w,"POST "); str(&w,u->path); str(&w,"/chat/completions HTTP/1.1\r\nHost: ");
    str(&w,u->host); put(&w,':'); number(&w,u->port);
    str(&w,"\r\nContent-Type: application/json\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\n");
    if(*key) { str(&w,"Authorization: Bearer "); str(&w,key); str(&w,"\r\n"); }
    str(&w,"Content-Length: "); number(&w,contentLen); str(&w,"\r\n\r\n");
    body(&w,model,m,count);
    return w.ok ? (int)w.n : -1;
}

typedef struct { unsigned offset, length; int status, chunked, hasLength; } Headers;
static int headers(const char *s,unsigned n,Headers *h) {
    unsigned i=0, end=0, a,b,k,v;
    h->length=0; h->hasLength=0; h->chunked=0;
    for(i=0;i+3<n;++i) if(equal(s+i,"\r\n\r\n",4)) { end=i+4; break; }
    if(!end) return n>4096 ? -1 : 0;
    if(end>4096 || n<12 || (!equal(s,"HTTP/1.0 ",9) && !equal(s,"HTTP/1.1 ",9))) return -1;
    for(i=9;i<12;++i) if(s[i]<'0' || s[i]>'9') return -1;
    h->status=(s[9]-'0')*100+(s[10]-'0')*10+s[11]-'0';
    i=12; while(i+1<end && !(s[i]=='\r' && s[i+1]=='\n')) ++i; i+=2;
    while(i+1<end && s[i]!='\r') {
        a=i; while(i<end && s[i]!=':' && s[i]!='\r') ++i;
        if(i>=end || s[i]!=':') return -1; b=i++;
        while(i<end && (s[i]==' ' || s[i]=='\t')) ++i; k=i;
        while(i+1<end && !(s[i]=='\r' && s[i+1]=='\n')) ++i;
        v=i; while(v>k && (s[v-1]==' ' || s[v-1]=='\t')) --v;
        if(b-a==14 && iequal(s+a,"Content-Length",14)) {
            unsigned long len=0; unsigned j; if(k==v || h->hasLength) return -1;
            for(j=k;j<v;++j) { if(s[j]<'0'||s[j]>'9'||len>MOSS_HTTP_MAX) return -1; len=len*10+s[j]-'0'; }
            if(len>MOSS_HTTP_MAX) return -1; h->length=len; h->hasLength=1;
        }
        if(b-a==17 && iequal(s+a,"Transfer-Encoding",17)) {
            if(v-k!=7 || !iequal(s+k,"chunked",7) || h->chunked) return -1;
            h->chunked=1;
        }
        if(b-a==16 && iequal(s+a,"Content-Encoding",16)) {
            if(v-k!=8 || !iequal(s+k,"identity",8)) return -1;
        }
        i+=2;
    }
    if(h->hasLength && h->chunked) return -1;
    h->offset=end; return 1;
}
static int hexval(char c) {
    if(c>='0' && c<='9') return c-'0';
    c=lower(c); return c>='a' && c<='f' ? c-'a'+10 : -1;
}
/* Validate the complete chunk stream before moving any bytes. */
static int chunks(const char *s,unsigned n,unsigned start,char *dest,int *decoded) {
    unsigned i=start, len, digits, j, out=0; int h;
    while(i<n) {
        len=0; digits=0;
        while(i<n && (h=hexval(s[i]))>=0) {
            if(len>MOSS_HTTP_MAX/16) return -1;
            len=len*16+(unsigned)h; ++i; ++digits;
        }
        if(!digits || len>MOSS_HTTP_MAX) return -1;
        if(i<n && s[i]==';') { while(i<n && s[i]!='\r') ++i; }
        if(i+1>=n) return 0;
        if(s[i]!='\r'||s[i+1]!='\n') return -1; i+=2;
        if(!len) {
            if(i+1<n && s[i]=='\r' && s[i+1]=='\n') { *decoded=(int)out; return 1; }
            for(j=i;j+3<n;++j) if(equal(s+j,"\r\n\r\n",4)) { *decoded=(int)out; return 1; }
            return 0;
        }
        if(n-i<len+2) return 0;
        if(s[i+len]!='\r'||s[i+len+1]!='\n') return -1;
        if(dest) for(j=0;j<len;++j) dest[out+j]=s[i+j];
        out+=len; i+=len+2;
    }
    return 0;
}
int moss_http_complete(const char *s,unsigned n,int eof) {
    Headers h; int r, decoded;
    r=headers(s,n,&h); if(r<=0) return eof ? -1 : r;
    if(h.chunked) { r=chunks(s,n,h.offset,0,&decoded); return !r && eof ? -1 : r; }
    if(h.hasLength) return n-h.offset>=h.length ? 1 : (eof ? -1 : 0);
    return eof ? 1 : 0;
}
int moss_http_body(char *s,unsigned n,int *status) {
    Headers h; unsigned i,len; int decoded;
    if(headers(s,n,&h)!=1) return -1; *status=h.status;
    if(h.chunked) {
        if(chunks(s,n,h.offset,0,&decoded)!=1) return -1;
        if(chunks(s,n,h.offset,s,&decoded)!=1) return -1;
        len=(unsigned)decoded;
    } else {
        len=h.hasLength ? h.length : n-h.offset;
        if(len>n-h.offset) return -1;
        for(i=0;i<len;++i) s[i]=s[h.offset+i];
    }
    s[len]=0; return (int)len;
}

/* Small recursive JSON reader. No token table or allocations; nesting is bounded. */
static const char *space(const char *p) { while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n') ++p; return p; }
static const char *jstring(const char *p) {
    unsigned i; if(*p++!='"') return 0;
    while(*p && *p!='"') {
        if((unsigned char)*p<32) return 0;
        if(*p++=='\\') {
            if(*p=='u') { ++p; for(i=0;i<4;++i) { if(hexval(*p)<0) return 0; ++p; } }
            else { if(!*p || !(*p=='"'||*p=='\\'||*p=='/'||*p=='b'||*p=='f'||*p=='n'||*p=='r'||*p=='t')) return 0; ++p; }
        }
    }
    return *p=='"' ? p+1 : 0;
}
static const char *skip(const char *p,unsigned depth) {
    char end; const char *q;
    if(depth>24) return 0; p=space(p);
    if(*p=='"') return jstring(p);
    if(*p=='{' || *p=='[') {
        end=*p=='{' ? '}' : ']'; p=space(p+1);
        if(*p==end) return p+1;
        for(;;) {
            if(end=='}') { p=jstring(p); if(!p) return 0; p=space(p); if(*p++!=':') return 0; }
            p=skip(p,depth+1); if(!p) return 0; p=space(p);
            if(*p==end) return p+1;
            if(*p++!=',') return 0; p=space(p);
        }
    }
    if(slen(p)>=4 && (equal(p,"true",4)||equal(p,"null",4))) return p+4;
    if(slen(p)>=5 && equal(p,"false",5)) return p+5;
    q=p; if(*p=='-') ++p;
    if(*p=='0') ++p;
    else { if(*p<'1'||*p>'9') return 0; do { ++p; } while(*p>='0'&&*p<='9'); }
    if(*p=='.') { ++p; if(*p<'0'||*p>'9') return 0; while(*p>='0'&&*p<='9') ++p; }
    if(*p=='e'||*p=='E') { ++p; if(*p=='+'||*p=='-') ++p; if(*p<'0'||*p>'9') return 0; while(*p>='0'&&*p<='9') ++p; }
    return p>q ? p : 0;
}
static const char *property(const char *p,const char *name) {
    const char *k,*end; unsigned n=slen(name);
    p=space(p); if(*p++!='{') return 0; p=space(p);
    while(*p && *p!='}') {
        k=p; end=jstring(p); if(!end) return 0;
        p=space(end); if(*p++!=':') return 0; p=space(p);
        if((unsigned)(end-k)==n+2 && equal(k+1,name,n)) return p;
        p=skip(p,0); if(!p) return 0; p=space(p); if(*p!=',') return 0; p=space(p+1);
    }
    return 0;
}
static unsigned long code4(const char *p) { unsigned long n=0; unsigned i; for(i=0;i<4;++i) n=n*16+(unsigned)hexval(p[i]); return n; }
static int decode(const char *p,char *out,unsigned cap,int *truncated) {
    unsigned used=0,n,i; unsigned long c,lo; char bytes[4];
    if(!cap || !p || *p!='"' || !jstring(p)) return -1;
    ++p; *truncated=0;
    while(*p!='"') {
        c=(unsigned char)*p++; n=1; bytes[0]=(char)c;
        if(c=='\\') {
            c=(unsigned char)*p++;
            if(c=='u') {
                c=code4(p); p+=4;
                if(c>=0xd800 && c<=0xdbff) {
                    if(p[0]!='\\'||p[1]!='u') return -1;
                    lo=code4(p+2); if(lo<0xdc00||lo>0xdfff) return -1;
                    p+=6; c=0x10000+((c-0xd800)<<10)+lo-0xdc00;
                } else if(c>=0xdc00 && c<=0xdfff) return -1;
                if(!c) return -1; /* Palm text fields cannot contain embedded NUL. */
                if(c<128) { n=1; bytes[0]=(char)c; }
                else if(c<2048) { n=2; bytes[0]=(char)(0xc0|(c>>6)); bytes[1]=(char)(0x80|(c&63)); }
                else if(c<65536) { n=3; bytes[0]=(char)(0xe0|(c>>12)); bytes[1]=(char)(0x80|((c>>6)&63)); bytes[2]=(char)(0x80|(c&63)); }
                else { n=4; bytes[0]=(char)(0xf0|(c>>18)); bytes[1]=(char)(0x80|((c>>12)&63)); bytes[2]=(char)(0x80|((c>>6)&63)); bytes[3]=(char)(0x80|(c&63)); }
            } else {
                if(c=='n') c='\n'; else if(c=='r') c='\r'; else if(c=='t') c='\t';
                else if(c=='b') c='\b'; else if(c=='f') c='\f'; bytes[0]=(char)c;
            }
        } else if(c>=128) {
            if(c>=0xc2 && c<=0xdf) n=2;
            else if(c>=0xe0 && c<=0xef) n=3;
            else if(c>=0xf0 && c<=0xf4) n=4;
            else return -1;
            for(i=1;i<n;++i) { if(((unsigned char)*p&0xc0)!=0x80) return -1; bytes[i]=*p++; }
            lo=(unsigned char)bytes[1];
            if((c==0xe0&&lo<0xa0)||(c==0xed&&lo>=0xa0)||(c==0xf0&&lo<0x90)||(c==0xf4&&lo>=0x90)) return -1;
        }
        if(!*truncated && used+n<cap) for(i=0;i<n;++i) out[used++]=bytes[i];
        else *truncated=1;
    }
    out[used]=0; return (int)used;
}
int moss_reply(const char *json,char *out,unsigned cap,int *truncated) {
    const char *p=skip(json,0); if(!p || *space(p)) return -1;
    p=property(json,"choices"); if(!p||*p!='[') return -1;
    p=property(space(p+1),"message"); if(!p) return -1;
    p=property(p,"content"); return decode(p,out,cap,truncated);
}
int moss_error(const char *json,char *out,unsigned cap) {
    const char *p=skip(json,0); int trunc;
    if(!p || *space(p)) return -1;
    p=property(json,"error"); if(!p) return -1;
    p=property(p,"message"); return decode(p,out,cap,&trunc);
}
