#include "app.h"
#include "../tls/MossTLS.h"
static UInt16 tlsRef;
static Boolean tlsOpened=false;
static void *tlsSession=NULL;
static UInt16 refnum, question=MOSS_NO_RECORD;
static NetSocketRef sock=-1;
static Boolean opened=false;
static UInt16 phase=0;
static UInt16 warmups=0;
static Boolean insecureSession=false;
static UInt32 started;
static Char *request=NULL, *wire=NULL;
static unsigned requestLen, sent, received;
static MossURL url;
static NetSocketAddrINType addr;

/* The 68K RSA path can spend minutes validating a 4096-bit CA chain. */
#define NET_REQUEST_TIMEOUT_SECONDS 900

Boolean NetBusy(void) { return phase!=0; }
static void showTlsStats(void) {
 MossTLSStats stats; Char status[40],value[16];
 if(!tlsSession || MossTLSGetStats(tlsRef,tlsSession,&stats)) return;
 StrCopy(status,"TLS "); StrIToA(value,stats.sent); StrCat(status,value); StrCat(status,"/");
 StrIToA(value,stats.received); StrCat(status,value); AppStatus(status);
}
static void closeNetwork(void) {
 Err err;
 if(tlsSession) { MossTLSDestroy(tlsRef,tlsSession); tlsSession=NULL; }
 if(tlsOpened) { if(!MossTLSClose(tlsRef)) SysLibRemove(tlsRef); tlsOpened=false; }
 if(sock>=0) { NetLibSocketClose(refnum,sock,SysTicksPerSecond(),&err); sock=-1; }
 if(opened) { NetLibClose(refnum,false); opened=false; }
 if(request) { MemPtrFree(request); request=NULL; }
 if(wire) { MemPtrFree(wire); wire=NULL; }
 phase=0;
}
static void failed(const Char *message,Err err) {
 Char text[220],number[16];
 StrNCopy(text,message,170); text[170]=0;
 if(err) { StrCat(text," (code "); StrIToA(number,(Int32)(UInt16)err); StrCat(text,number); StrCat(text,")"); }
 if(question!=MOSS_NO_RECORD) StoreState(question,MSG_FAILED);
 closeNetwork(); AppRefresh(); AppStatus("Failed"); AppAlert(text);
}
void NetCancel(void) {
 if(!phase) return;
 if(question!=MOSS_NO_RECORD) StoreState(question,MSG_FAILED);
 closeNetwork(); AppRefresh(); AppStatus("Cancelled");
}
void NetStart(UInt16 record) {
 Char *context, *current; MossMessageHeader head;
 MossMessage selected[MOSS_CONTEXT_MAX+1]; UInt16 indices[MOSS_CONTEXT_MAX];
 unsigned n=0,used=0,i,len; Int16 k; int result;
 if(phase) return;
 if(!moss_url(gPrefs.url,&url)) { AppAlert("Enter a valid API Base URL in settings."); return; }
 if(!gPrefs.model[0]) { AppAlert("Enter the model ID in API settings."); return; }
 question=record;
 context=MemPtrNew(4096); current=MemPtrNew(MOSS_REPLY_MAX+1);
 request=MemPtrNew(MOSS_REQUEST_MAX);
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
 requestLen=(unsigned)result; sent=received=0;
 if(record!=MOSS_NO_RECORD && !StoreState(record,MSG_WAITING)) { failed("Cannot save request state.",0); return; }
 insecureSession=false; warmups=0; phase=1; started=TimGetTicks(); AppRefresh(); AppStatus("Connecting");
}
static void finish(void) {
 int status=0,len,truncated=0; Char *reply,code[20],error[220];
 len=moss_http_body(wire,received,&status);
 if(len<0) { failed("Invalid HTTP response.",0); return; }
 if(status<200 || status>=300) {
  StrCopy(error,"HTTP "); StrIToA(code,status); StrCat(error,code);
  if(status==401||status==403) StrCat(error,": check API key.");
  else if(status==404) StrCat(error,": check Base URL / model.");
  else if(status==429) StrCat(error,": rate limit or quota.");
  else if(status>=500) StrCat(error,": API server error.");
  else StrCat(error,": API request failed.");
  failed(error,0); return;
 }
 reply=MemPtrNew(MOSS_REPLY_MAX+64);
 if(!reply) { failed("Not enough memory for the reply.",0); return; }
 len=moss_reply(wire,reply,MOSS_REPLY_MAX+1,&truncated);
 if(len<=0) {
  if(moss_error(wire,error,sizeof(error))>0) { MemPtrFree(reply); failed(error,0); }
  else { MemPtrFree(reply); failed("API returned no supported text answer.",0); }
  return;
 }
 if(question==MOSS_NO_RECORD) {
  MemPtrFree(reply); closeNetwork(); AppRefresh(); AppAlert("API connection OK. The configured model returned a text answer."); return;
 }
 if(truncated) StrCat(reply,"\n[Truncated at device limit]");
 if(StoreAdd(true,MSG_OK,reply)==MOSS_NO_RECORD) {
  MemPtrFree(reply); failed("Cannot save reply. Free device storage.",0); return;
 }
 MemPtrFree(reply);
 StoreState(question,MSG_OK); closeNetwork(); AppRefresh(); AppStatus(insecureSession?"Ready (TLS dev)":"Ready");
}
void NetPump(void) {
 Err err=0; UInt16 iferr=0; Int16 count; UInt32 ticks=SysTicksPerSecond(); int complete;
 if(!phase) return;
 EvtResetAutoOffTimer();
 if((UInt32)(TimGetTicks()-started)>NET_REQUEST_TIMEOUT_SECONDS*ticks) { failed("Request timed out. Retry from the Chat menu.",0); return; }
 if(tlsSession) {
  UInt16 state,detail;
  err=MossTLSPump(tlsRef,tlsSession,&state,&detail);
  if(err) {
   Char message[160],value[16]; UInt32 x509=0;
   MossTLSLastX509Error(tlsRef,tlsSession,&x509);
   StrCopy(message,"TLS failed. TLS code "); StrIToA(value,detail); StrCat(message,value);
   StrCat(message,", X509 "); StrIToA(value,(Int32)x509); StrCat(message,value);
   {
    MossTLSStats stats;
    if(!MossTLSGetStats(tlsRef,tlsSession,&stats)) {
     StrCat(message,". TX "); StrIToA(value,stats.sent); StrCat(message,value);
     StrCat(message," RX "); StrIToA(value,stats.received); StrCat(message,value);
     StrCat(message," CPU "); StrIToA(value,stats.workTicks/ticks); StrCat(message,value);
     StrCat(message,"s elapsed "); StrIToA(value,(TimGetTicks()-started)/ticks); StrCat(message,value); StrCat(message,"s");
    }
   }
   failed(message,err); return;
  }
  if(phase==6) {
   MossTLSStats stats; Char status[40],value[16];
   MossTLSGetStats(tlsRef,tlsSession,&stats);
   StrIToA(status,stats.sent); StrCat(status,"/");
   StrIToA(value,stats.received); StrCat(status,value); StrCat(status," s");
   StrIToA(value,stats.state); StrCat(status,value); StrCat(status," e");
   StrIToA(value,stats.lastIOError); StrCat(status,value); AppStatus(status);
   if((state&MT_STATE_READY) && (state&MT_STATE_RECONNECT)) {
    if(++warmups>2) { failed("Server certificates changed repeatedly. Retry.",0); return; }
    MossTLSDestroy(tlsRef,tlsSession); tlsSession=NULL;
    NetLibSocketClose(refnum,sock,ticks,&err); sock=-1;
    phase=2; AppStatus("Reconnecting");
   }
   else if(state&MT_STATE_READY) { insecureSession=(state&MT_STATE_INSECURE)!=0; phase=4; AppStatus("Sending"); }
   else if(state&MT_STATE_CLOSED) failed("TLS closed before handshake completed.",0);
   return;
  }
 }
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
  if(url.tls) {
   if(!tlsOpened) {
   if(SysLibFind(MOSS_TLS_NAME,&tlsRef)) {
    err=SysLibLoad('libr',MOSS_TLS_CREATOR,&tlsRef);
    if(err) { failed("Install MossTLS.prc first.",err); return; }
   }
   err=MossTLSOpen(tlsRef); if(err) { failed("Cannot open TLS library.",err); return; }
   tlsOpened=true;
   }
   err=MossTLSCreate(tlsRef,refnum,sock,url.host,&tlsSession);
   if(err) { failed(err==mtErrEntropy?"Install a unique MossTLSEntropy.pdb seed.":"Cannot create TLS session.",err); return; }
   phase=6; AppStatus("TLS handshake"); return;
  }
  phase=4; AppStatus("Sending"); return;
 }
 if(phase==4) {
  unsigned remaining=requestLen-sent; if(remaining>512) remaining=512;
  if(tlsSession) {
   UInt16 wrote=0; err=MossTLSWrite(tlsRef,tlsSession,request+sent,(UInt16)remaining,&wrote);
   if(err) { failed("TLS send failed.",err); return; }
   count=(Int16)wrote;
   showTlsStats();
   if(!count) return;
  } else count=NetLibSend(refnum,sock,request+sent,(UInt16)remaining,0,NULL,0,1,&err);
  if(count<0) { if(err==netErrTimeout||err==netErrWouldBlock) return; failed("Sending failed.",err); return; }
  if(count==0) { failed("Connection closed while sending.",0); return; }
  sent+=count; if(sent<requestLen) return;
  MemPtrFree(request); request=NULL;
  wire=MemPtrNew(MOSS_HTTP_MAX+1); if(!wire) { failed("Not enough memory to receive answer.",0); return; }
  wire[0]=0; phase=5; AppStatus("Waiting"); return;
 }
 if(tlsSession) {
  UInt16 read=0;
  showTlsStats();
  err=MossTLSRead(tlsRef,tlsSession,wire+received,(UInt16)((MOSS_HTTP_MAX-received)>512?512:MOSS_HTTP_MAX-received),&read);
  if(err) { failed("TLS receive failed.",err); return; }
  count=(Int16)read;
  if(!count) return;
 } else count=NetLibReceive(refnum,sock,wire+received,(UInt16)((MOSS_HTTP_MAX-received)>512?512:MOSS_HTTP_MAX-received),0,NULL,NULL,1,&err);
 if(count<0) { if(err==netErrTimeout||err==netErrWouldBlock) return; failed("Connection lost while receiving.",err); return; }
 received+=count; wire[received]=0;
 complete=moss_http_complete(wire,received,count==0);
 if(complete<0) { failed("Unsupported or incomplete HTTP response.",0); return; }
 if(complete>0) { finish(); return; }
 if(received==MOSS_HTTP_MAX) failed("API response exceeds 16 KB. Request a shorter answer.",0);
}
