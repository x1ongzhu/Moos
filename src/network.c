#include "app.h"
static UInt16 refnum, question=MOSS_NO_RECORD;
static NetSocketRef sock=-1;
static Boolean opened=false;
static UInt16 phase=0;
static UInt32 started, idleSince;
static Boolean idle=false, reusedRequest=false;
static Char *request=NULL;
static MossStream *response=NULL;
static UInt32 paintedAt;
static unsigned paintedLength;
static unsigned requestLen, sent;
static MossURL url;
static NetSocketAddrINType addr;

#define NET_REQUEST_TIMEOUT_SECONDS 180
#define NET_IDLE_SECONDS 60

Boolean NetBusy(void) { return phase!=0; }
Boolean NetConnected(void) { return idle; }
static void closeTransport(void) {
 Err err;
 if(sock>=0) { NetLibSocketClose(refnum,sock,SysTicksPerSecond(),&err); sock=-1; }
 if(opened) { NetLibClose(refnum,false); opened=false; }
 idle=false;
}
static void releaseRequest(void) {
 if(request) { MemPtrFree(request); request=NULL; }
 if(response) { MemPtrFree(response); response=NULL; }
 phase=0;
}
static void closeNetwork(void) { closeTransport(); releaseRequest(); }
/* Retry only before any request plaintext has been accepted by socket. */
static Boolean reconnectUnused(void) {
 if(!reusedRequest || sent) return false;
 closeTransport(); reusedRequest=false;
 phase=1; AppStatus("Reconnecting"); return true;
}
static void failed(const Char *message,Err err) {
 Char text[220],number[16];
 StrNCopy(text,message,170); text[170]=0;
 if(err) { StrCat(text," (code "); StrIToA(number,(Int32)(UInt16)err); StrCat(text,number); StrCat(text,")"); }
 if(question!=MOSS_NO_RECORD) StoreState(question,MSG_FAILED);
 closeNetwork(); AppRefresh(); AppStatus("Failed"); AppAlert(text);
}
void NetCancel(void) {
 if(!phase) { closeTransport(); return; }
 if(question!=MOSS_NO_RECORD) StoreState(question,MSG_FAILED);
 closeNetwork(); AppRefresh(); AppStatus("Cancelled");
}
void NetStart(UInt16 record) {
 Char *context, *current; MossMessageHeader head;
 MossMessage selected[MOSS_CONTEXT_MAX+1]; UInt16 indices[MOSS_CONTEXT_MAX];
 unsigned n=0,used=0,i,len; Int16 k; int result; MossURL next; Boolean reuse;
 if(phase) return;
 if(!moss_url(gPrefs.url,&next)) { AppAlert("Enter a valid API Base URL in settings."); return; }
 if(next.tls) { AppAlert("Use an http:// relay URL. Configure upstream HTTPS on the relay."); return; }
 if(!gPrefs.model[0]) { AppAlert("Enter the model ID in API settings."); return; }
 reuse=idle && (UInt32)(TimGetTicks()-idleSince)<NET_IDLE_SECONDS*(UInt32)SysTicksPerSecond()
  && next.port==url.port && next.tls==url.tls && !StrCompare(next.host,url.host);
 if(!reuse) closeTransport();
 url=next; question=record;
 context=MemPtrNew(4096); current=MemPtrNew(MOSS_REPLY_MAX+1);
 request=MemPtrNew(MOSS_REQUEST_MAX);
 if(reuse && (!context || !current || !request)) {
  /* A retained connection must not prevent a request on a small Palm heap. */
  closeTransport(); reuse=false;
  if(!context) context=MemPtrNew(4096);
  if(!current) current=MemPtrNew(MOSS_REPLY_MAX+1);
  if(!request) request=MemPtrNew(MOSS_REQUEST_MAX);
 }
 if(!context || !current || !request) {
  if(context) MemPtrFree(context); if(current) MemPtrFree(current);
  failed("Not enough memory for a request.",0); return;
 }
 /* Walk complete turns backwards, keeping a bounded recent context. */
 for(k=(Int16)gMessageCount-1;record!=MOSS_NO_RECORD && k>=0 && n<MOSS_CONTEXT_MAX;--k) {
  if(gMessages[k]>=record) continue;
  if(!StoreRead(gMessages[k],&head,current,MOSS_REPLY_MAX+1)) continue;
  if(head.state!=MSG_OK) continue;
  len=StrLen(current)+1; if(used+len>3000) break;
  indices[n++]=gMessages[k]; used+=len;
 }
 used=0;
 for(i=0;i<n;++i) {
  if(!StoreRead(indices[n-i-1],&head,current,MOSS_REPLY_MAX+1)) break;
  len=StrLen(current)+1; MemMove(context+used,current,len);
  selected[i].role=head.assistant ? "assistant" : "user";
  selected[i].text=context+used; used+=len;
 }
 n=i;
 /* Avoid a leading orphan assistant when the history window is trimmed. */
 if(n && selected[0].role[0]=='a') { for(i=1;i<n;++i) selected[i-1]=selected[i]; --n; }
 if(record==MOSS_NO_RECORD) StrCopy(current,"Reply with OK.");
 else if(!StoreRead(record,&head,current,MOSS_REPLY_MAX+1)) {
  MemPtrFree(context); MemPtrFree(current); failed("Cannot read saved question.",0); return;
 }
 selected[n].role="user"; selected[n].text=current; ++n;
 result=moss_request(request,MOSS_REQUEST_MAX,&url,gPrefs.key,gPrefs.model,selected,n);
 MemPtrFree(context); MemPtrFree(current);
 if(result<0) { failed("Request exceeds memory limit or API key is invalid.",0); return; }
 requestLen=(unsigned)result; sent=0;
 if(record!=MOSS_NO_RECORD && !StoreState(record,MSG_WAITING)) { failed("Cannot save request state.",0); return; }
 reusedRequest=reuse; idle=false; phase=reuse?4:1;
 started=TimGetTicks(); AppRefresh(); AppStatus(reuse?"Reusing connection":"Connecting");
}
static void finish(void) {
 Boolean keep=response->reusable!=0;
 if(question==MOSS_NO_RECORD) {
  closeNetwork(); AppRefresh(); AppAlert("API connection OK. The configured model returned a text answer."); return;
 }
 if(response->truncated) StrCat(response->answer,"\n[Truncated at device limit]");
 if(StoreAdd(true,MSG_OK,response->answer)==MOSS_NO_RECORD) {
  failed("Cannot save reply. Free device storage.",0); return;
 }
 StoreState(question,MSG_OK);
 if(keep) { releaseRequest(); idle=true; idleSince=TimGetTicks(); }
 else closeNetwork();
 AppRefresh(); AppStatus(reusedRequest?"Ready (reused)":"Ready");
}
void NetPump(void) {
 Err err=0; UInt16 iferr=0; Int16 count; UInt32 ticks=SysTicksPerSecond(); int complete; Char input[512];
 if(!phase) {
  if(idle && (UInt32)(TimGetTicks()-idleSince)>=NET_IDLE_SECONDS*ticks) closeTransport();
  return;
 }
 EvtResetAutoOffTimer();
 if((UInt32)(TimGetTicks()-started)>NET_REQUEST_TIMEOUT_SECONDS*ticks) { failed("Request timed out. Retry from the Chat menu.",0); return; }
 if(phase==1) {
  if(SysLibFind("Net.lib",&refnum)) { failed("Net Library unavailable. Configure device networking first.",0); return; }
  err=NetLibOpen(refnum,&iferr);
  opened=(err==errNone || err==netErrAlreadyOpen);
  if(!opened || iferr) { failed("Network could not open. Check Network preferences.",iferr?iferr:err); return; }
  phase=2; AppStatus("Resolving"); return;
 }
 if(phase==2) {
  NetHostInfoBufType *buf; NetHostInfoPtr host;
  MemSet(&addr,sizeof(addr),0); addr.family=netSocketAddrINET; addr.port=NetHToNS(url.port);
  addr.addr=NetLibAddrAToIN(refnum,url.host);
  if(addr.addr==(NetIPAddr)-1) {
   buf=MemPtrNew(sizeof(*buf)); if(!buf) { failed("Not enough memory for DNS.",0); return; }
   host=NetLibGetHostByName(refnum,url.host,buf,2*ticks,&err);
   if(!host || !host->addrListP || !host->addrListP[0]) { MemPtrFree(buf); failed("DNS failed. Check the API hostname.",err); return; }
   MemMove(&addr.addr,host->addrListP[0],sizeof(addr.addr)); MemPtrFree(buf);
  }
  sock=NetLibSocketOpen(refnum,netSocketAddrINET,netSocketTypeStream,0,ticks,&err);
  if(sock<0) { failed("Cannot create network socket.",err); return; }
  phase=3; return;
 }
 if(phase==3) {
  if(NetLibSocketConnect(refnum,sock,(NetSocketAddrType *)&addr,sizeof(addr),2*ticks,&err)<0) {
   failed("Cannot connect to API host / port.",err); return;
  }
  phase=4; AppStatus("Sending"); return;
 }
 if(phase==4) {
  unsigned remaining=requestLen-sent; if(remaining>512) remaining=512;
  count=NetLibSend(refnum,sock,request+sent,(UInt16)remaining,0,NULL,0,1,&err);
  if(count<0) { if(err==netErrTimeout||err==netErrWouldBlock) return; if(reconnectUnused()) return; failed("Sending failed.",err); return; }
  if(count==0) { if(reconnectUnused()) return; failed("Connection closed while sending.",0); return; }
  sent+=count; if(sent<requestLen) return;
  MemPtrFree(request); request=NULL;
  response=MemPtrNew(sizeof(*response));
  if(!response) { failed("Not enough memory to receive answer.",0); return; }
  moss_stream_init(response); paintedLength=0; paintedAt=0;
  phase=5; AppStatus("Waiting"); return;
 }
 count=NetLibReceive(refnum,sock,input,sizeof(input),0,NULL,NULL,1,&err);
 if(count<0) { if(err==netErrTimeout||err==netErrWouldBlock) return; failed("Connection lost while receiving.",err); return; }
 complete=moss_stream_feed(response,input,(unsigned)count,count==0);
 if(complete<0) {
  Char message[220],code[16];
  StrCopy(message,"");
  if(response->status>=300) {
   StrCopy(message,"HTTP "); StrIToA(code,response->status); StrCat(message,code); StrCat(message,": ");
  }
  StrCat(message,response->error); failed(message,0); return;
 }
 if(complete>0) { finish(); return; }
 if(question!=MOSS_NO_RECORD && response->used!=paintedLength &&
    (!paintedLength || (UInt32)(TimGetTicks()-paintedAt)>=ticks/10+1)) {
  AppStream(response->answer); paintedLength=response->used; paintedAt=TimGetTicks();
 }

}
