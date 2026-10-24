#define MOSS_TLS_IMPLEMENTATION
#include "MossTLS.h"
#include <System/NetMgr.h>
#include "core_abi.h"
#define STACK_SIZE 16384UL
#define RUNTIME_NAME "MossTLS Runtime"
#define SEED_NAME "MossTLS Entropy"
typedef struct Session {
 struct Session *next;
 UInt16 netRef;
 Int16 socket;
 void *context,*stack;
 UInt32 sent,received,workTicks; UInt16 lastIOError;
} Session;
typedef struct {
 UInt16 users;
 DmOpenRef db;
 MemHandle code;
 MTCore entry;
 Session *sessions;
 MTValidationCache cache;
} Globals;
extern UInt32 MTInvoke(MTCore entry,UInt32 op,void *context,void *arg,void *stackTop);
static Globals *globals(UInt16 ref) { return SysLibTblEntry(ref)->globalsP; }
static Session *valid(Globals *g,void *s) {
 Session *p=g?g->sessions:NULL; while(p) { if(p==s) return p; p=p->next; } return NULL;
}
static UInt32 call(Globals *g,Session *s,UInt32 op,void *arg) {
 return MTInvoke(g->entry,op,s?s->context:NULL,arg,(Char *)s->stack+STACK_SIZE);
}
static void releaseCode(Globals *g) {
 if(g->code) { MemHandleUnlock(g->code); DmReleaseResource(g->code); g->code=NULL; }
 if(g->db) { DmCloseDatabase(g->db); g->db=NULL; }
 { LocalID id=DmFindDatabase(0,RUNTIME_NAME); if(id) DmDeleteDatabase(0,id); }
}
static Err loadCode(Globals *g) {
 LocalID id; DmOpenRef source; MemHandle raw,meta; UInt32 size,*rel,entry,count,i,value; Char *code,*original;
 if(g->entry) return errNone;
 source=DmOpenDatabaseByTypeCreator('libr',MOSS_TLS_CREATOR,dmModeReadOnly);
 if(!source) return mtErrLibrary;
 raw=DmGet1Resource('mtCD',1000); meta=DmGet1Resource('mtRL',1000);
 if(!raw || !meta) { if(raw) DmReleaseResource(raw); if(meta) DmReleaseResource(meta); DmCloseDatabase(source); return mtErrLibrary; }
 size=MemHandleSize(raw); rel=MemHandleLock(meta); entry=rel[0]; count=rel[1];
 if(entry>=size || MemHandleSize(meta)!=8+count*4) { MemHandleUnlock(meta); DmReleaseResource(meta); DmReleaseResource(raw); DmCloseDatabase(source); return mtErrLibrary; }
 id=DmFindDatabase(0,RUNTIME_NAME); if(id) DmDeleteDatabase(0,id);
 if(DmCreateDatabase(0,RUNTIME_NAME,MOSS_TLS_CREATOR,'MTRT',true)) goto failed;
 id=DmFindDatabase(0,RUNTIME_NAME); g->db=DmOpenDatabase(0,id,dmModeReadWrite);
 if(!g->db) goto failed;
 g->code=DmNewResource(g->db,'mtCD',1000,size); if(!g->code) goto failed;
 code=MemHandleLock(g->code); original=MemHandleLock(raw);
 if(DmWrite(code,0,original,size)) { MemHandleUnlock(raw); goto failed; }
 MemHandleUnlock(raw);
 for(i=0;i<count;++i) {
  UInt32 offset=rel[2+i]; if(offset+4>size) goto failed;
  MemMove(&value,code+offset,4); if(value>=size) goto failed;
  value+=(UInt32)code; if(DmWrite(code,offset,&value,4)) goto failed;
 }
 g->entry=(MTCore)(code+entry);
 MemHandleUnlock(meta); DmReleaseResource(meta); DmReleaseResource(raw); DmCloseDatabase(source);
 return errNone;
failed:
 MemHandleUnlock(meta); DmReleaseResource(meta); DmReleaseResource(raw); DmCloseDatabase(source);
 releaseCode(g); return mtErrMemory;
}
Err MossTLSOpen(UInt16 ref) {
 Globals *g=globals(ref);
 if(!g) {
  g=MemPtrNew(sizeof(*g)); if(!g) return mtErrMemory;
  MemPtrSetOwner(g,0); MemSet(g,sizeof(*g),0); SysLibTblEntry(ref)->globalsP=g;
 }
 ++g->users; return errNone;
}
Err MossTLSClose(UInt16 ref) {
 Globals *g=globals(ref); if(!g) return errNone;
 if(g->users>1) { --g->users; return mtErrBusy; }
 if(g->sessions) return mtErrBusy;
 releaseCode(g); MemPtrFree(g); SysLibTblEntry(ref)->globalsP=NULL; return errNone;
}
Err MossTLSSleep(UInt16 ref) { (void)ref; return errNone; }
Err MossTLSWake(UInt16 ref) { (void)ref; return errNone; }
Err MossTLSVersion(UInt16 ref,UInt32 *version) { (void)ref; if(!version) return mtErrParam; *version=0x00010000UL; return errNone; }
Err MossTLSCreate(UInt16 ref,UInt16 netRef,Int16 socket,const Char *host,void **out) {
 Globals *g=globals(ref); Session *s; Err err; DmOpenRef db; MemHandle seed; MTInit init; UInt32 bytes,seconds; LocalID id;
 if(!g || !out || !host || !*host) return mtErrParam; *out=NULL;
 err=loadCode(g); if(err) return err;
 id=DmFindDatabase(0,SEED_NAME); if(!id) return mtErrEntropy;
 db=DmOpenDatabase(0,id,dmModeReadWrite); if(!db) return mtErrEntropy;
 seed=DmGetRecord(db,0); if(!seed || MemHandleSize(seed)!=32) { if(seed) DmReleaseRecord(db,0,false); DmCloseDatabase(db); return mtErrEntropy; }
 MemMove(init.seed,MemHandleLock(seed),32); MemHandleUnlock(seed);
 init.host=host; seconds=TimGetSeconds();
 seconds+=(Int32)(Int16)PrefGetPreference(prefMinutesWestOfGMT)*60L;
 /* Palm epoch is 1904-01-01, BearSSL uses days since 0000-01-01. */
 init.days=695421UL+seconds/86400UL; init.seconds=seconds%86400UL;
 init.cache=&g->cache;
 s=MemPtrNew(sizeof(*s)); if(!s) { DmReleaseRecord(db,0,false); DmCloseDatabase(db); return mtErrMemory; }
 MemPtrSetOwner(s,0); MemSet(s,sizeof(*s),0); s->netRef=netRef; s->socket=socket;
 s->stack=MemPtrNew(STACK_SIZE); if(!s->stack) goto no_memory;
 MemPtrSetOwner(s->stack,0);
 bytes=MTInvoke(g->entry,MT_SIZE,NULL,NULL,(Char *)s->stack+STACK_SIZE);
 s->context=MemPtrNew(bytes); if(!s->context) goto no_memory;
 MemPtrSetOwner(s->context,0);
 if(!call(g,s,MT_INIT,&init)) { err=mtErrHandshake; goto failed; }
 /* Commit the ratcheted seed before any ClientHello or application data leaves. */
 err=DmWrite(MemHandleLock(seed),0,init.seed,32); MemHandleUnlock(seed);
 MemSet(init.seed,32,0); DmReleaseRecord(db,0,!err); DmCloseDatabase(db);
 if(err) { MemPtrFree(s->context); MemPtrFree(s->stack); MemPtrFree(s); return mtErrEntropy; }
 s->next=g->sessions; g->sessions=s; *out=s; return errNone;
no_memory: err=mtErrMemory;
failed:
 MemSet(init.seed,32,0); DmReleaseRecord(db,0,false); DmCloseDatabase(db);
 if(s->context) MemPtrFree(s->context); if(s->stack) MemPtrFree(s->stack); MemPtrFree(s); return err;
}
Err MossTLSPump(UInt16 ref,void *opaque,UInt16 *state,UInt16 *detail) {
 Globals *g=globals(ref); Session *s=valid(g,opaque); MTBuffer b; MTWord n; UInt32 flags,before; Int16 got; Err err=0;
 if(!s || !state || !detail) return mtErrParam;
 flags=call(g,s,MT_STATE,NULL); *state=(UInt16)flags; *detail=0;
 if(flags&MT_STATE_CLOSED) { *detail=(UInt16)call(g,s,MT_ERROR,NULL); return *detail?mtErrHandshake:errNone; }
 if(flags&MT_STATE_SENDREC) {
  call(g,s,MT_SENDREC,&b); if(b.length>1024) b.length=1024;
  got=NetLibSend(s->netRef,s->socket,b.data,(UInt16)b.length,0,NULL,0,1,&err);
  if(got>0) { n=(UInt32)got; s->sent+=n; call(g,s,MT_SENDREC_ACK,&n); }
  else if(got==0) {
   s->lastIOError=err; *detail=(UInt16)err;
   if(err==netErrTimeout || err==netErrWouldBlock) return errNone;
   /* Some transports report a broken pipe as zero bytes and no error.
    * Consume a pending TLS alert before reporting the transport failure. */
   if(err==errNone && (flags&MT_STATE_RECVREC)) {
    call(g,s,MT_RECVREC,&b); if(b.length>2048) b.length=2048;
    if(b.length) {
     got=NetLibReceive(s->netRef,s->socket,b.data,(UInt16)b.length,0,NULL,NULL,1,&err);
     if(got>0) {
      n=(UInt32)got; s->received+=n; call(g,s,MT_RECVREC_ACK,&n);
      flags=call(g,s,MT_STATE,NULL); *state=(UInt16)flags;
      if(flags&MT_STATE_CLOSED) { *detail=(UInt16)call(g,s,MT_ERROR,NULL); return *detail?mtErrHandshake:mtErrIO; }
      return errNone;
     }
    }
   }
   return mtErrIO;
  }
  else if(err!=netErrTimeout && err!=netErrWouldBlock) { *detail=err; return mtErrIO; }
 } else if(flags&MT_STATE_RECVREC) {
  call(g,s,MT_RECVREC,&b); if(b.length>2048) b.length=2048;
  got=NetLibReceive(s->netRef,s->socket,b.data,(UInt16)b.length,0,NULL,NULL,1,&err);
  if(got>0) { n=(UInt32)got; s->received+=n; before=TimGetTicks(); call(g,s,MT_RECVREC_ACK,&n); s->workTicks+=TimGetTicks()-before; }
  else if(got==0) {
   s->lastIOError=err; *detail=(UInt16)err;
   if(err==netErrTimeout || err==netErrWouldBlock) return errNone;
   return mtErrIO;
  }
  else if(err!=netErrTimeout && err!=netErrWouldBlock) { *detail=err; return mtErrIO; }
 }
 s->lastIOError=err;
 flags=call(g,s,MT_STATE,NULL); *state=(UInt16)flags;
 if(flags&MT_STATE_CLOSED) { *detail=(UInt16)call(g,s,MT_ERROR,NULL); return *detail?mtErrHandshake:errNone; }
 return errNone;
}
Err MossTLSWrite(UInt16 ref,void *opaque,const void *data,UInt16 count,UInt16 *written) {
 Globals *g=globals(ref); Session *s=valid(g,opaque); MTBuffer b; MTWord n;
 if(!s || !data || !written) return mtErrParam; *written=0;
 call(g,s,MT_SENDAPP,&b); if(!b.length) return errNone;
 n=count; if(n>b.length) n=b.length; MemMove(b.data,data,n);
 call(g,s,MT_SENDAPP_ACK,&n); call(g,s,MT_FLUSH,NULL); *written=(UInt16)n; return errNone;
}
Err MossTLSRead(UInt16 ref,void *opaque,void *data,UInt16 cap,UInt16 *received) {
 Globals *g=globals(ref); Session *s=valid(g,opaque); MTBuffer b; MTWord n;
 if(!s || !data || !received) return mtErrParam; *received=0;
 call(g,s,MT_RECVAPP,&b); if(!b.length) return errNone;
 n=cap; if(n>b.length) n=b.length; MemMove(data,b.data,n); call(g,s,MT_RECVAPP_ACK,&n); *received=(UInt16)n; return errNone;
}
Err MossTLSDestroy(UInt16 ref,void *opaque) {
 Globals *g=globals(ref); Session *s=valid(g,opaque),**link;
 if(!s) return mtErrParam;
 link=&g->sessions; while(*link!=s) link=&(*link)->next; *link=s->next;
 /* Context contains traffic secrets; erase before freeing. */
 { UInt32 bytes=call(g,s,MT_SIZE,NULL); MemSet(s->context,bytes,0); }
 MemPtrFree(s->context); MemSet(s->stack,STACK_SIZE,0); MemPtrFree(s->stack); MemPtrFree(s); return errNone;
}
Err MossTLSLastX509Error(UInt16 ref,void *opaque,UInt32 *error) {
 Globals *g=globals(ref); Session *s=valid(g,opaque); if(!s || !error) return mtErrParam;
 *error=call(g,s,MT_X509_ERROR,NULL); return errNone;
}

Err MossTLSGetStats(UInt16 ref,void *opaque,MossTLSStats *stats) {
 Globals *g=globals(ref); Session *s=valid(g,opaque); if(!s || !stats) return mtErrParam;
 stats->sent=s->sent; stats->received=s->received; stats->workTicks=s->workTicks; stats->state=(UInt16)call(g,s,MT_STATE,NULL); stats->lastIOError=s->lastIOError; return errNone;
}
