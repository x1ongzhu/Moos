#include "app.h"
static DmOpenRef chats, messages;
MossPrefs gPrefs;
MossConversation gConversation;
UInt16 gConversationRecord=MOSS_NO_RECORD;
UInt16 gMessages[MOSS_MAX_MESSAGES], gMessageCount=0;
static DmOpenRef openDB(const Char *name, UInt32 type) {
 LocalID id=DmFindDatabase(0,name);
 if(!id) {
  if(DmCreateDatabase(0,name,MOSS_CREATOR,type,false)) return NULL;
  id=DmFindDatabase(0,name);
 }
 return DmOpenDatabase(0,id,dmModeReadWrite);
}
Boolean StoreOpen(void) {
 UInt16 size=sizeof(gPrefs); Int16 version;
 MemSet(&gPrefs,sizeof(gPrefs),0);
 version=PrefGetAppPreferences(MOSS_CREATOR,0,&gPrefs,&size,true);
 if(version!=1 || size!=sizeof(gPrefs)) {
  MemSet(&gPrefs,sizeof(gPrefs),0); gPrefs.version=1;
  StrCopy(gPrefs.url,"https://api.deepseek.com/v1");
 }
 gPrefs.url[192]=0; gPrefs.model[63]=0; gPrefs.key[191]=0;
 /* Optional one-shot configuration import. Keys stay in RAM unless user opts in. */
 {
  LocalID id=DmFindDatabase(0,"Moss Test Config");
  if(id) {
   DmOpenRef config=DmOpenDatabase(0,id,dmModeReadOnly);
   if(config) {
    MemHandle h=DmQueryRecord(config,0);
    if(h && MemHandleSize(h)==sizeof(gPrefs)) {
     void *p=MemHandleLock(h); MemMove(&gPrefs,p,sizeof(gPrefs)); MemHandleUnlock(h);
     gPrefs.url[192]=0; gPrefs.model[63]=0; gPrefs.key[191]=0; gPrefs.rememberKey=false;
    }
    DmCloseDatabase(config);
   }
   DmDeleteDatabase(0,id);
  }
 }
 chats=openDB("Moss Chats",'MCHT'); messages=openDB("Moss Messages",'MMSG');
 if(!chats || !messages) return false;
 /* An interrupted foreground request is never resumed silently. */
 {
  UInt16 i; MossMessageHeader h; Char text[2];
  for(i=0;i<DmNumRecords(messages);++i)
   if(StoreRead(i,&h,text,sizeof(text)) && h.state==MSG_WAITING) StoreState(i,MSG_FAILED);
 }
 if(DmNumRecords(chats)) return StoreLoad(DmNumRecords(chats)-1);
 return StoreNew();
}
Boolean StoreSaveConversation(void) {
 MemHandle h; Err err;
 if(!chats || gConversationRecord==MOSS_NO_RECORD) return false;
 h=DmGetRecord(chats,gConversationRecord); if(!h) return false;
 gConversation.modified=TimGetSeconds();
 err=DmWrite(MemHandleLock(h),0,&gConversation,sizeof(gConversation));
 MemHandleUnlock(h); DmReleaseRecord(chats,gConversationRecord,err==errNone);
 return err==errNone;
}
void StoreClose(void) {
 Char key[192];
 if(chats) StoreSaveConversation();
 StrCopy(key,gPrefs.key); if(!gPrefs.rememberKey) gPrefs.key[0]=0;
 PrefSetAppPreferences(MOSS_CREATOR,0,1,&gPrefs,sizeof(gPrefs),true);
 StrCopy(gPrefs.key,key); MemSet(key,sizeof(key),0);
 if(chats) DmCloseDatabase(chats); if(messages) DmCloseDatabase(messages);
 chats=messages=NULL;
}
void StoreIndex(void) {
 UInt16 i; MossMessageHeader h; MemHandle m; void *p;
 gMessageCount=0;
 for(i=0;i<DmNumRecords(messages) && gMessageCount<MOSS_MAX_MESSAGES;++i) {
  m=DmQueryRecord(messages,i); if(!m || MemHandleSize(m)<sizeof(h)+1) continue;
  p=MemHandleLock(m); MemMove(&h,p,sizeof(h)); MemHandleUnlock(m);
  if(h.conversation==gConversation.id) gMessages[gMessageCount++]=i;
 }
}
Boolean StoreLoad(UInt16 record) {
 MemHandle h=DmQueryRecord(chats,record); void *p;
 if(!h || MemHandleSize(h)!=sizeof(gConversation)) return false;
 p=MemHandleLock(h); MemMove(&gConversation,p,sizeof(gConversation)); MemHandleUnlock(h);
 gConversation.title[39]=0; gConversation.draft[MOSS_MESSAGE_MAX]=0;
 gConversationRecord=record; StoreIndex(); return true;
}
Boolean StoreNew(void) {
 UInt16 idx=dmMaxRecordIndex; MemHandle h; Err err;
 MossConversation *fresh;
 if(DmNumRecords(chats)>=MOSS_MAX_CHATS) return false;
 fresh=MemPtrNew(sizeof(*fresh)); if(!fresh) return false;
 MemSet(fresh,sizeof(*fresh),0); fresh->id=TimGetSeconds();
 /* IDs remain unique even when several chats are created in the same second. */
 {
  UInt16 i; MemHandle old; MossConversation *c;
  for(i=0;i<DmNumRecords(chats);++i) {
   old=DmQueryRecord(chats,i); if(!old || MemHandleSize(old)!=sizeof(*fresh)) continue;
   c=MemHandleLock(old); if(c->id>=fresh->id) fresh->id=c->id+1; MemHandleUnlock(old);
  }
 }
 fresh->modified=TimGetSeconds(); StrCopy(fresh->title,"New chat");
 h=DmNewRecord(chats,&idx,sizeof(*fresh));
 if(!h) { MemPtrFree(fresh); return false; }
 err=DmWrite(MemHandleLock(h),0,fresh,sizeof(*fresh));
 MemHandleUnlock(h); DmReleaseRecord(chats,idx,err==errNone); MemPtrFree(fresh);
 if(err) { DmRemoveRecord(chats,idx); return false; }
 return StoreLoad(idx);
}
UInt16 StoreAdd(Boolean assistant,UInt8 state,const Char *text) {
 UInt16 idx=dmMaxRecordIndex; MemHandle h; void *p; MossMessageHeader header; Err err;
 if(gMessageCount>=MOSS_MAX_MESSAGES) return MOSS_NO_RECORD;
 MemSet(&header,sizeof(header),0); header.conversation=gConversation.id;
 header.created=TimGetSeconds(); header.assistant=assistant; header.state=state;
 h=DmNewRecord(messages,&idx,sizeof(header)+StrLen(text)+1); if(!h) return MOSS_NO_RECORD;
 p=MemHandleLock(h); err=DmWrite(p,0,&header,sizeof(header));
 if(!err) err=DmWrite(p,sizeof(header),text,StrLen(text)+1);
 MemHandleUnlock(h); DmReleaseRecord(messages,idx,err==errNone);
 if(err) { DmRemoveRecord(messages,idx); return MOSS_NO_RECORD; }
 StoreIndex(); StoreSaveConversation(); return idx;
}
Boolean StoreState(UInt16 record,UInt8 state) {
 MemHandle h=DmGetRecord(messages,record); Err err;
 if(!h) return false;
 err=DmWrite(MemHandleLock(h),OffsetOf(MossMessageHeader,state),&state,1);
 MemHandleUnlock(h); DmReleaseRecord(messages,record,err==errNone); return !err;
}
Boolean StoreRead(UInt16 record,MossMessageHeader *header,Char *out,UInt16 cap) {
 MemHandle h=DmQueryRecord(messages,record); Char *p; UInt32 len;
 if(!h || !cap || MemHandleSize(h)<sizeof(*header)+1) return false;
 p=MemHandleLock(h); MemMove(header,p,sizeof(*header));
 len=MemHandleSize(h)-sizeof(*header); if(len>=cap) len=cap-1;
 MemMove(out,p+sizeof(*header),len); out[len]=0;
 MemHandleUnlock(h); return true;
}
Boolean StoreDelete(void) {
 Int16 i; MemHandle h; MossMessageHeader head; void *p; UInt16 idx=gConversationRecord;
 for(i=(Int16)DmNumRecords(messages)-1;i>=0;--i) {
  h=DmQueryRecord(messages,(UInt16)i); if(!h || MemHandleSize(h)<sizeof(head)) continue;
  p=MemHandleLock(h); MemMove(&head,p,sizeof(head)); MemHandleUnlock(h);
  if(head.conversation==gConversation.id && DmRemoveRecord(messages,(UInt16)i)) return false;
 }
 if(DmRemoveRecord(chats,idx)) return false;
 gConversationRecord=MOSS_NO_RECORD;
 if(DmNumRecords(chats)) return StoreLoad(DmNumRecords(chats)-1);
 return StoreNew();
}
UInt16 StoreList(Char titles[MOSS_MAX_CHATS][40],UInt16 *records) {
 UInt16 i,n=0; MemHandle h; MossConversation *p;
 /* Reverse creation order for this initial build. */
 for(i=DmNumRecords(chats);i>0 && n<MOSS_MAX_CHATS;--i) {
  h=DmQueryRecord(chats,i-1); if(!h||MemHandleSize(h)!=sizeof(*p)) continue;
  p=MemHandleLock(h); StrNCopy(titles[n],p->title,39); titles[n][39]=0;
  MemHandleUnlock(h); records[n++]=i-1;
 }
 return n;
}
