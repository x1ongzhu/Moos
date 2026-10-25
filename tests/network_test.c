/* Exercise the actual Palm network state machine with deterministic transports.
 * These are host simulation tests, not real AI calls. */
#include <assert.h>
#include "../src/network.c"
MossPrefs gPrefs;
MossConversation gConversation;
UInt16 gConversationRecord,gMessages[MOSS_MAX_MESSAGES],gMessageCount;
static UInt32 clockTicks;
static int connects,closes,answers,alerts,writes,dead,failAfterWrite,ready;
static char mockResponse[4096],lastStatus[80];
static int failAllocation,previews,mockEOF;
static unsigned readOffset,receivePiece=512;
void *TestAlloc(size_t n) {
 if(failAllocation) { failAllocation=0; return NULL; }
 return malloc(n);
}
UInt32 TimGetTicks(void) { return clockTicks; }
UInt16 SysTicksPerSecond(void) { return 100; }
void EvtResetAutoOffTimer(void) {}
Err SysLibFind(const char *s,UInt16 *ref) { *ref=1; return 0; }
Err SysLibLoad(UInt32 a,UInt32 b,UInt16 *ref) { *ref=1; return 0; }
Err SysLibRemove(UInt16 ref) { return 0; }
Err NetLibOpen(UInt16 ref,UInt16 *error) { *error=0; return 0; }
Err NetLibClose(UInt16 ref,Boolean force) { return 0; }
Err NetLibSocketClose(UInt16 ref,NetSocketRef socket,UInt32 ticks,Err *error) { ++closes; ready=0; return 0; }
NetIPAddr NetLibAddrAToIN(UInt16 ref,const char *s) { return 1; }
NetHostInfoPtr NetLibGetHostByName(UInt16 a,const char *b,NetHostInfoBufType *c,UInt32 d,Err *e) { return NULL; }
NetSocketRef NetLibSocketOpen(UInt16 a,int b,int c,int d,UInt32 e,Err *f) { return 1; }
Int16 NetLibSocketConnect(UInt16 a,NetSocketRef b,NetSocketAddrType *c,UInt16 d,UInt32 e,Err *f) {
 ++connects; dead=0; return 0;
}
static Int16 sendBytes(UInt16 n,Err *error) {
 if(dead) { *error=99; return -1; }
 ++writes; ready=1; readOffset=0; if(failAfterWrite) dead=1; return n;
}
Int16 NetLibSend(UInt16 a,NetSocketRef b,const void *c,UInt16 n,int d,void *e,int f,UInt32 g,Err *error) { return sendBytes(n,error); }
Int16 NetLibReceive(UInt16 a,NetSocketRef b,void *out,UInt16 n,int d,void *e,void *f,UInt32 g,Err *error) {
 unsigned len=(unsigned)strlen(mockResponse)-readOffset;
 if(dead) { *error=99; return -1; }
 if(!ready) { if(mockEOF) return 0; *error=netErrTimeout; return -1; }
 if(len>n) len=n; if(len>receivePiece) len=receivePiece;
 memcpy(out,mockResponse+readOffset,len); readOffset+=len;
 if(readOffset==strlen(mockResponse)) ready=0;
 return (Int16)len;
}
Boolean StoreRead(UInt16 a,MossMessageHeader *h,Char *out,UInt16 cap) { memset(h,0,sizeof(*h)); strcpy(out,"Test"); return true; }
Boolean StoreState(UInt16 a,UInt8 state) { return true; }
UInt16 StoreAdd(Boolean assistant,UInt8 state,const Char *text) { assert(!strcmp(text,"OK")); ++answers; return 1; }
void AppStatus(const Char *s) { strcpy(lastStatus,s); }
void AppRefresh(void) {}
void AppStream(const Char *s) { assert(*s); ++previews; }
void AppAlert(const Char *s) { ++alerts; }
static void reply(const char *extra) {
 snprintf(mockResponse,sizeof(mockResponse),"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n%s\r\nOK",extra);
}
static void runRequest(void) {
 int i; NetStart(0);
 for(i=0;i<2000 && NetBusy();++i) { NetPump(); ++clockTicks; }
 assert(!NetBusy());
}
static void reset(const char *base) {
 NetCancel(); clockTicks=0; connects=closes=answers=alerts=writes=dead=failAfterWrite=ready=0;
 previews=0; mockEOF=0; receivePiece=512; readOffset=0;
 strcpy(gPrefs.url,base); strcpy(gPrefs.model,"test"); reply("");
}
static void streamReply(int done) {
 snprintf(mockResponse,sizeof(mockResponse),"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nO\r\n1\r\nK\r\n%s",done?"0\r\n\r\n":"");
 mockEOF=!done; receivePiece=1;
}
static void streamingChecks(void) {
 int i;
 reset("http://one.test/v1"); streamReply(1); runRequest();
 assert(previews>0 && answers==1 && !alerts && NetConnected());
 runRequest(); assert(answers==2 && connects==1);
 reset("http://one.test/v1"); streamReply(0); runRequest();
 assert(previews>0 && answers==0 && alerts==1 && !NetConnected());
 reset("http://one.test/v1"); streamReply(1); NetStart(0);
 for(i=0;i<1000 && !previews;++i) { NetPump(); ++clockTicks; }
 assert(previews && NetBusy() && !answers); NetCancel();
 assert(!NetBusy() && !answers && !NetConnected());
 reset("http://one.test/v1"); streamReply(1); NetStart(0);
 for(i=0;i<1000 && !previews;++i) { NetPump(); ++clockTicks; }
 dead=1; NetPump(); assert(alerts==1 && !answers && connects==1);
}
int main(void) {
 const char *base="http://one.test/v1";
 {
  reset(base); runRequest(); runRequest();
  assert(answers==2 && connects==1 && NetConnected() && !alerts);
  assert(strstr(lastStatus,"reused"));
  clockTicks+=NET_IDLE_SECONDS*100; NetPump(); assert(!NetConnected() && closes==1);
  runRequest(); assert(connects==2 && answers==3);
  NetCancel(); assert(!NetConnected());
  reset(base); runRequest(); dead=1; runRequest();
  assert(connects==2 && answers==2 && writes==2 && !alerts);
  reset(base); runRequest(); failAfterWrite=1; runRequest();
  assert(connects==1 && writes==2 && answers==1 && alerts==1 && !NetConnected());
  reset(base); reply("Connection: close\r\n"); runRequest(); runRequest();
  assert(connects==2 && answers==2 && !NetConnected());
  reset(base); runRequest(); strcpy(gPrefs.url,"http://two.test/v1"); runRequest();
  assert(connects==2 && answers==2);
  reset(base); runRequest(); failAllocation=1; runRequest();
  assert(connects==2 && answers==2 && !alerts);
  reset("https://one.test/v1"); runRequest();
  assert(connects==0 && writes==0 && alerts==1);
  reset(base); runRequest(); strcpy(gPrefs.url,"http://one.test:8080/v1"); runRequest();
  assert(connects==2 && answers==2);
  reset(base); runRequest(); NetStart(0); NetCancel();
  assert(!NetBusy() && !NetConnected() && writes==1);
  reset(base); runRequest(); clockTicks+=NET_IDLE_SECONDS*100; runRequest(); assert(connects==2);
 }
 streamingChecks(); NetCancel(); puts("Network reuse checks passed (HTTP relay transport simulation)"); return 0;
}
