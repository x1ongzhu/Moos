#ifndef MOSS_PROTOCOL_H
#define MOSS_PROTOCOL_H
/* Portable C89 core, shared by the Palm client and host protocol tests. */
#define MOSS_MESSAGE_MAX 1024
#define MOSS_REPLY_MAX 4096
#define MOSS_HTTP_MAX 16384
#define MOSS_REQUEST_MAX 8192
#define MOSS_CONTEXT_MAX 6

typedef struct { const char *role; const char *text; } MossMessage;
typedef struct { char host[128]; char path[256]; unsigned short port; int tls; } MossURL;
int moss_url(const char *base, MossURL *url);
int moss_request(char *out, unsigned cap, const MossURL *url,
                 const char *key, const char *model,
                 const MossMessage *messages, unsigned count);
/* HTTP completion: 0=incomplete, 1=complete, -1=invalid. eof is peer close. */
int moss_http_complete(const char *wire, unsigned size, int eof);
/* Decode HTTP framing in place. Returns body length or -1; status is HTTP code. */
int moss_http_body(char *wire, unsigned size, int *status);
/* Returns decoded UTF-8 bytes or -1. truncated distinguishes capacity limit. */
int moss_reply(const char *json, char *out, unsigned cap, int *truncated);
int moss_error(const char *json, char *out, unsigned cap);
#endif
