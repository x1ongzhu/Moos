#include "protocol.h"

void moss_display_ascii(char *text) {
    unsigned char *p=(unsigned char *)text,*out=p,c;
    unsigned need,i; unsigned long cp;
    while(*p) {
        c=*p++;
        if(c<128) { *out++=c; continue; }
        if(c>=0xc2 && c<=0xdf) { need=1; cp=c&31; }
        else if(c>=0xe0 && c<=0xef) { need=2; cp=c&15; }
        else if(c>=0xf0 && c<=0xf4) { need=3; cp=c&7; }
        else { *out++='?'; continue; }
        for(i=0;i<need && (*p&0xc0)==0x80;++i) cp=(cp<<6)|(*p++&63);
        if(i!=need) { *out++='?'; continue; }
        if((cp>=0x2018UL && cp<=0x201bUL) || cp==0x2032UL || cp==0xff07UL) *out++='\'';
        else if((cp>=0x201cUL && cp<=0x201fUL) || cp==0x2033UL || cp==0xff02UL) *out++='"';
        else if((cp>=0x2010UL && cp<=0x2015UL) || cp==0x2212UL) *out++='-';
        else if(cp==0x2026UL) { *out++='.'; *out++='.'; *out++='.'; }
        else if(cp==0xa0UL || cp==0x202fUL) *out++=' ';
        else if(cp==0x2022UL || cp==0xb7UL) *out++='*';
        else *out++='?';
    }
    *out=0;
}

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
    str(w,"{\"model\":"); quote(w,model); str(w,",\"stream\":true,\"messages\":[");
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
    str(&w,"\r\nContent-Type: application/json\r\nAccept: text/plain\r\nAccept-Encoding: identity\r\nConnection: keep-alive\r\n");
    if(*key) { str(&w,"Authorization: Bearer "); str(&w,key); str(&w,"\r\n"); }
    str(&w,"Content-Length: "); number(&w,contentLen); str(&w,"\r\n\r\n");
    body(&w,model,m,count);
    return w.ok ? (int)w.n : -1;
}

typedef struct { unsigned offset; unsigned long length; int status, chunked, hasLength, persistent, close, text; } Headers;
static int headers(const char *s,unsigned n,Headers *h,unsigned long limit) {
    unsigned i=0, end=0, a,b,k,v;
    h->length=0; h->hasLength=0; h->chunked=0; h->persistent=0; h->close=0; h->text=0;
    for(i=0;i+3<n;++i) if(equal(s+i,"\r\n\r\n",4)) { end=i+4; break; }
    if(!end) return n>4096 ? -1 : 0;
    if(end>4096 || n<12 || (!equal(s,"HTTP/1.0 ",9) && !equal(s,"HTTP/1.1 ",9))) return -1;
    for(i=9;i<12;++i) if(s[i]<'0' || s[i]>'9') return -1;
    h->persistent=s[7]=='1';
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
            for(j=k;j<v;++j) { if(s[j]<'0'||s[j]>'9'||len>limit) return -1; len=len*10+s[j]-'0'; }
            if(len>limit) return -1; h->length=len; h->hasLength=1;
        }
        if(b-a==17 && iequal(s+a,"Transfer-Encoding",17)) {
            if(v-k!=7 || !iequal(s+k,"chunked",7) || h->chunked) return -1;
            h->chunked=1;
        }
        if(b-a==10 && iequal(s+a,"Connection",10)) {
            unsigned j=k, t, z;
            while(j<v) {
                while(j<v && (s[j]==' ' || s[j]=='\t' || s[j]==',')) ++j;
                t=j; while(j<v && s[j]!=',') ++j; z=j;
                while(z>t && (s[z-1]==' ' || s[z-1]=='\t')) --z;
                if(z-t==5 && iequal(s+t,"close",5)) h->close=1;
                if(z-t==10 && iequal(s+t,"keep-alive",10)) h->persistent=1;
            }
        }
        if(b-a==12 && iequal(s+a,"Content-Type",12)) {
            if(v-k>=10 && iequal(s+k,"text/plain",10) &&
               (v-k==10 || s[k+10]==';' || s[k+10]==' ')) h->text=1;
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
static int chunks(const char *s,unsigned n,unsigned start,char *dest,int *decoded,unsigned *consumed) {
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
            if(i+1<n && s[i]=='\r' && s[i+1]=='\n') { *decoded=(int)out; if(consumed) *consumed=i+2; return 1; }
            for(j=i;j+3<n;++j) if(equal(s+j,"\r\n\r\n",4)) { *decoded=(int)out; if(consumed) *consumed=j+4; return 1; }
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
    r=headers(s,n,&h,MOSS_HTTP_MAX); if(r<=0) return eof ? -1 : r;
    if(h.chunked) { r=chunks(s,n,h.offset,0,&decoded,0); return !r && eof ? -1 : r; }
    if(h.hasLength) return n-h.offset>=h.length ? 1 : (eof ? -1 : 0);
    return eof ? 1 : 0;
}
int moss_http_reusable(const char *s,unsigned n) {
    Headers h; int decoded; unsigned consumed=0;
    if(headers(s,n,&h,MOSS_HTTP_MAX)!=1 || !h.persistent || h.close || h.status<200) return 0;
    if(h.chunked) return chunks(s,n,h.offset,0,&decoded,&consumed)==1 && consumed==n;
    return h.hasLength && n-h.offset==h.length;
}
int moss_http_body(char *s,unsigned n,int *status) {
    Headers h; unsigned i,len; int decoded;
    if(headers(s,n,&h,MOSS_HTTP_MAX)!=1) return -1; *status=h.status;
    if(h.chunked) {
        if(chunks(s,n,h.offset,0,&decoded,0)!=1) return -1;
        if(chunks(s,n,h.offset,s,&decoded,0)!=1) return -1;
        len=(unsigned)decoded;
    } else {
        len=h.hasLength ? h.length : n-h.offset;
        if(len>n-h.offset) return -1;
        for(i=0;i<len;++i) s[i]=s[h.offset+i];
    }
    s[len]=0; return (int)len;
}

void moss_stream_init(MossStream *s) {
    unsigned i; char *p=(char *)s;
    for(i=0;i<sizeof(*s);++i) p[i]=0;
}
static int stream_error(MossStream *s,const char *message) {
    copy(s->error,sizeof(s->error),message,slen(message)); s->failed=1; s->reusable=0; return -1;
}
static int stream_append(MossStream *s,const char *bytes,unsigned n) {
    unsigned i;
    if(s->status<200 || s->status>=300) {
        if(s->errorUsed+n<sizeof(s->error)) {
            for(i=0;i<n;++i) s->error[s->errorUsed++]=bytes[i];
            s->error[s->errorUsed]=0;
        }
    } else if(!s->truncated && s->used+n<=MOSS_REPLY_MAX) {
        for(i=0;i<n;++i) s->answer[s->used++]=bytes[i];
        s->answer[s->used]=0;
    } else s->truncated=1;
    return 0;
}
static int stream_body_byte(MossStream *s,char value) {
    unsigned char c=(unsigned char)value,first,second;
    if(!s->utf8Used) {
        if(!c) return stream_error(s,"NUL byte in response.");
        if(c<128) return stream_append(s,&value,1);
        if(c>=0xc2 && c<=0xdf) s->utf8Need=2;
        else if(c>=0xe0 && c<=0xef) s->utf8Need=3;
        else if(c>=0xf0 && c<=0xf4) s->utf8Need=4;
        else return stream_error(s,"Invalid UTF-8 response.");
        s->utf8[s->utf8Used++]=value; return 0;
    }
    if((c&0xc0)!=0x80) return stream_error(s,"Invalid UTF-8 continuation.");
    s->utf8[s->utf8Used++]=value;
    if(s->utf8Used<s->utf8Need) return 0;
    first=(unsigned char)s->utf8[0]; second=(unsigned char)s->utf8[1];
    if((first==0xe0 && second<0xa0) || (first==0xed && second>=0xa0) ||
       (first==0xf0 && second<0x90) || (first==0xf4 && second>=0x90))
        return stream_error(s,"Invalid UTF-8 code point.");
    s->utf8Used=0; return stream_append(s,s->utf8,s->utf8Need);
}
static int stream_finish(MossStream *s) {
    if(s->utf8Used) return stream_error(s,"Response ended inside a UTF-8 character.");
    if(s->status<200 || s->status>=300) {
        s->failed=1; s->reusable=0;
        if(!s->errorUsed) return stream_error(s,"API request failed.");
        return -1;
    }
    if(!s->used) return stream_error(s,"API returned an empty text answer.");
    s->complete=1; return 1;
}
int moss_stream_feed(MossStream *s,const char *data,unsigned size,int eof) {
    unsigned i,j,digits; int r,h; unsigned long length; char c; Headers head;
    if(s->failed) return -1;
    for(i=0;i<size;++i) {
        if(s->complete) return stream_error(s,"Extra bytes after HTTP response.");
        if(++s->total>MOSS_STREAM_WIRE_MAX) return stream_error(s,"Streaming response exceeds wire limit.");
        c=data[i];
        if(!s->headersDone) {
            if(s->headerUsed==MOSS_HEADER_MAX) return stream_error(s,"HTTP headers too large.");
            s->header[s->headerUsed++]=c;
            if(s->headerUsed<4 || !equal(s->header+s->headerUsed-4,"\r\n\r\n",4)) continue;
            if(headers(s->header,s->headerUsed,&head,MOSS_STREAM_WIRE_MAX)!=1)
                return stream_error(s,"Invalid HTTP response headers.");
            s->status=head.status; s->headersDone=1;
            if(head.status>=200 && head.status<300 && (!head.text || (!head.chunked && !head.hasLength)))
                return stream_error(s,"Expected framed text/plain from updated relay.");
            s->reusable=head.persistent && !head.close && (head.chunked || head.hasLength);
            s->mode=head.chunked?1:(head.hasLength?2:3); s->remaining=head.length;
            s->headerUsed=0;
            if(s->mode==2 && !s->remaining && stream_finish(s)<0) return -1;
            continue;
        }
        if(s->mode==1) {
            if(s->chunkState==0 || s->chunkState==4) {
                if(s->headerUsed==MOSS_HEADER_MAX || (s->chunkState==0 && s->headerUsed==128))
                    return stream_error(s,"HTTP chunk framing too large.");
                s->header[s->headerUsed++]=c;
                if(s->chunkState==4 && ++s->trailerUsed>MOSS_HEADER_MAX)
                    return stream_error(s,"HTTP trailers too large.");
                if(c!='\n') continue;
                if(s->headerUsed<2 || s->header[s->headerUsed-2]!='\r')
                    return stream_error(s,"Invalid HTTP chunk line.");
                if(s->chunkState==4) {
                    if(s->headerUsed==2 && stream_finish(s)<0) return -1;
                    s->headerUsed=0; continue;
                }
                length=0; digits=0;
                for(j=0;j+2<s->headerUsed && (h=hexval(s->header[j]))>=0;++j) {
                    if(length>MOSS_STREAM_WIRE_MAX/16) return stream_error(s,"HTTP chunk too large.");
                    length=length*16+(unsigned)h; ++digits;
                }
                if(!digits || length>MOSS_STREAM_WIRE_MAX ||
                   (j+2!=s->headerUsed && s->header[j]!=';'))
                    return stream_error(s,"Invalid HTTP chunk size.");
                s->chunkRemaining=length; s->chunkState=length?1:4; s->headerUsed=0; continue;
            }
            if(s->chunkState==2 || s->chunkState==3) {
                if(c!=(s->chunkState==2?'\r':'\n')) return stream_error(s,"Invalid chunk terminator.");
                s->chunkState=s->chunkState==2?3:0; continue;
            }
            if(stream_body_byte(s,c)<0) return -1;
            if(!--s->chunkRemaining) s->chunkState=2;
        } else {
            if(stream_body_byte(s,c)<0) return -1;
            if(s->mode==2 && !--s->remaining && stream_finish(s)<0) return -1;
        }
    }
    if(eof) s->reusable=0;
    if(eof && !s->complete) {
        s->reusable=0;
        if(s->headersDone && s->mode==3) { r=stream_finish(s); if(r<0) return r; }
        else return stream_error(s,"Connection closed before HTTP response completed.");
    }
    return s->complete?1:0;
}
