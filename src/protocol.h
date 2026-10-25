#ifndef MOSS_PROTOCOL_H
#define MOSS_PROTOCOL_H
/* Portable C89 core, shared by the Palm client and host protocol tests. */
#define MOSS_MESSAGE_MAX 1024
#define MOSS_REPLY_MAX 4096
#define MOSS_HTTP_MAX 16384
#define MOSS_REQUEST_MAX 8192
#define MOSS_CONTEXT_MAX 6
#define MOSS_HEADER_MAX 1024
#define MOSS_STREAM_WIRE_MAX 2097152UL
/* In-place ASCII presentation; stored UTF-8 text remains untouched. */
void moss_display_ascii(char *text);

typedef struct { const char *role; const char *text; } MossMessage;
typedef struct { char host[128]; char path[256]; unsigned short port; int tls; } MossURL;
int moss_url(const char *base, MossURL *url);
int moss_request(char *out, unsigned cap, const MossURL *url,
                 const char *key, const char *model,
                 const MossMessage *messages, unsigned count);
/* HTTP completion: 0=incomplete, 1=complete, -1=invalid. eof is peer close. */
int moss_http_complete(const char *wire, unsigned size, int eof);
/* True only for an exactly framed response permitting a subsequent request. */
int moss_http_reusable(const char *wire, unsigned size);
/* Decode HTTP framing in place. Returns body length or -1; status is HTTP code. */
int moss_http_body(char *wire, unsigned size, int *status);
/* Incremental HTTP text-body decoder; no SSE/JSON response buffers. */
typedef struct {
 char header[MOSS_HEADER_MAX+1], answer[MOSS_REPLY_MAX+64], error[160], utf8[4];
 unsigned headerUsed,used,errorUsed,trailerUsed,utf8Used,utf8Need;
 unsigned long total,remaining,chunkRemaining;
 int status,headersDone,mode,chunkState,complete,reusable,truncated,failed;
} MossStream;
void moss_stream_init(MossStream *stream);
/* -1=invalid/interrupted, 0=receiving, 1=complete HTTP text response. */
int moss_stream_feed(MossStream *stream,const char *data,unsigned size,int eof);
#endif
