#include "app.h"
static UInt16 showing=0;
static UInt32 displayedConversation=0;
static Char titles[MOSS_MAX_CHATS][40];
static Char *titlePointers[MOSS_MAX_CHATS];
static UInt16 chatRecords[MOSS_MAX_CHATS],chatCount;
static Boolean stopping=false, clearKey=false;
static FieldType *field(UInt16 id) {
 FormType *f=FrmGetActiveForm(); UInt16 i=FrmGetObjectIndex(f,id);
 return i==frmInvalidObjectId ? NULL : FrmGetObjectPtr(f,i);
}
static const Char *text(UInt16 id) { Char *p=FldGetTextPtr(field(id)); return p?p:""; }
static void setText(UInt16 id,const Char *s) {
 FieldType *f=field(id); MemHandle h,old; Char *p;
 if(!f) return;
 h=MemHandleNew(StrLen(s)+1); if(!h) return;
 p=MemHandleLock(h); StrCopy(p,s); MemHandleUnlock(h);
 old=FldGetTextHandle(f); FldSetTextHandle(f,h); if(old) MemHandleFree(old);
}
static void asciiDisplay(Char *s) {
 Char *p=s,*out=s;
 while(*p) {
  if((UInt8)*p<128) *out++=*p++;
  else { *out++='?'; ++p; while(((UInt8)*p&0xc0)==0x80) ++p; }
 }
 *out=0;
}
void AppAlert(const Char *s) { FrmCustomAlert(ALERT_INFO,s,"",""); }
void AppStatus(const Char *s) {
 FormType *f=FrmGetActiveForm(); RectangleType r; UInt16 i;
 if(!f || FrmGetFormId(f)!=FORM_CHAT) return;
 r.topLeft.x=73; r.topLeft.y=94; r.extent.x=86; r.extent.y=12;
 WinEraseRectangle(&r,0); WinDrawChars(s,StrLen(s),74,95);
 i=FrmGetObjectIndex(f,BUTTON_SEND);
 CtlSetLabel(FrmGetObjectPtr(f,i),NetBusy()?"Cancel":"Send");
}
static void showMessage(void) {
 Char *buf; Char prefix[80],num[16]; MossMessageHeader h; unsigned offset;
 if(!gMessageCount) { setText(FIELD_REPLY,"Welcome to Moss.\n\nSet your API in Chat > API settings.\nEnglish / ASCII input in this build."); return; }
 if(showing>=gMessageCount) showing=gMessageCount-1;
 buf=MemPtrNew(MOSS_REPLY_MAX+160); if(!buf) return;
 StrCopy(prefix,""); StrIToA(num,showing+1); StrCat(prefix,num); StrCat(prefix,"/");
 StrIToA(num,gMessageCount); StrCat(prefix,num); StrCat(prefix,"  ");
 if(!StoreRead(gMessages[showing],&h,buf+80,MOSS_REPLY_MAX+64)) { MemPtrFree(buf); return; }
 StrCat(prefix,h.assistant?"AI":"You");
 if(h.state==MSG_FAILED) StrCat(prefix," [Retry available]");
 else if(h.state==MSG_WAITING) StrCat(prefix," [Waiting]");
 StrCat(prefix,"\n\n"); offset=StrLen(prefix);
 MemMove(buf+offset,buf+80,StrLen(buf+80)+1); MemMove(buf,prefix,offset);
 asciiDisplay(buf); setText(FIELD_REPLY,buf); MemPtrFree(buf);
}
void AppRefresh(void) {
 FormType *f=FrmGetActiveForm();
 if(!f || FrmGetFormId(f)!=FORM_CHAT) return;
 showing=gMessageCount?gMessageCount-1:0;
 FrmSetTitle(f,gConversation.title); showMessage();
 FldDrawField(field(FIELD_REPLY)); AppStatus(NetBusy()?"Working":"Ready");
}
static Boolean saveDraft(void) {
 const Char *s;
 if(FrmGetActiveFormID()!=FORM_CHAT || displayedConversation!=gConversation.id) return true;
 s=text(FIELD_INPUT); StrNCopy(gConversation.draft,s,MOSS_MESSAGE_MAX); gConversation.draft[MOSS_MESSAGE_MAX]=0;
 return StoreSaveConversation();
}
static void newChat(void) {
 if(!saveDraft() || !StoreNew()) { AppAlert("Cannot create chat. Maximum 32 chats; check device storage."); return; }
 FrmGotoForm(FORM_CHAT);
}
static void sendQuestion(void) {
 const Char *p=text(FIELD_INPUT),*scan; UInt16 record;
 if(NetBusy()) { NetCancel(); return; }
 scan=p; while(*scan==' '||*scan=='\n'||*scan=='\r'||*scan=='\t') ++scan;
 if(!*scan) { AppAlert("Enter a question first."); return; }
 for(scan=p;*scan;++scan) if((UInt8)*scan>=128) { AppAlert("This build supports ASCII input only."); return; }
 if(gMessageCount>=MOSS_MAX_MESSAGES-1) { AppAlert("This chat has reached 128 messages. Create a new chat."); return; }
 if(!gPrefs.model[0]) { saveDraft(); FrmGotoForm(FORM_SETTINGS); return; }
 if(!saveDraft()) { AppAlert("Cannot save draft. Free device storage."); return; }
 record=StoreAdd(false,MSG_FAILED,p);
 if(record==MOSS_NO_RECORD) { AppAlert("Cannot save question. Free device storage."); return; }
 if(gMessageCount==1) { StrNCopy(gConversation.title,p,39); gConversation.title[39]=0; }
 gConversation.draft[0]=0; StoreSaveConversation(); setText(FIELD_INPUT,""); FldDrawField(field(FIELD_INPUT));
 NetStart(record); AppRefresh();
}
static void retry(void) {
 Char tiny[2]; MossMessageHeader h;
 if(!gMessageCount) { AppAlert("No question to retry."); return; }
 if(StoreRead(gMessages[gMessageCount-1],&h,tiny,sizeof(tiny)) && !h.assistant && h.state==MSG_FAILED) NetStart(gMessages[gMessageCount-1]);
 else AppAlert("The last message is not a failed question.");
}
static void renameChat(void) {
 FormType *previous=FrmGetActiveForm(),*dialog=FrmInitForm(FORM_RENAME); UInt16 result;
 if(!dialog) return;
 FrmSetActiveForm(dialog); setText(FIELD_TITLE,gConversation.title);
 FrmSetFocus(dialog,FrmGetObjectIndex(dialog,FIELD_TITLE)); result=FrmDoDialog(dialog);
 if(result==BUTTON_SAVE && *text(FIELD_TITLE)) {
  StrNCopy(gConversation.title,text(FIELD_TITLE),39); gConversation.title[39]=0;
  if(!StoreSaveConversation()) AppAlert("Could not save title.");
 }
 FrmDeleteForm(dialog); FrmSetActiveForm(previous); FrmDrawForm(previous); AppRefresh();
}
static Boolean menu(UInt16 id) {
 UInt16 focus; FieldType *f=NULL;
 if(id==MENU_COPY || id==MENU_PASTE || id==MENU_SELECT) {
  focus=FrmGetFocus(FrmGetActiveForm());
  if(focus!=noFocus && FrmGetObjectType(FrmGetActiveForm(),focus)==frmFieldObj)
   f=FrmGetObjectPtr(FrmGetActiveForm(),focus);
  if(f) { if(id==MENU_COPY) FldCopy(f); else if(id==MENU_PASTE) FldPaste(f); else FldSetSelection(f,0,FldGetTextLength(f)); }
  return true;
 }
 if(NetBusy()) { AppAlert("Cancel the active request first."); return true; }
 if(FrmGetActiveFormID()!=FORM_CHAT) return true;
 switch(id) {
 case MENU_SETTINGS: saveDraft(); FrmGotoForm(FORM_SETTINGS); return true;
 case MENU_RETRY: retry(); return true;
 case MENU_TEST: NetStart(MOSS_NO_RECORD); return true;
 case MENU_RENAME: renameChat(); return true;
 case MENU_DELETE:
  if(FrmAlert(ALERT_DELETE)==0) { if(StoreDelete()) FrmGotoForm(FORM_CHAT); else AppAlert("Could not delete chat."); }
  return true;
 case MENU_ABOUT: AppAlert("Moss 0.1.0\nPalm OS 3.1+\nOpenAI-compatible text chat.\nASCII display; native TLS library."); return true;
 }
 return false;
}
static Boolean chatEvent(EventType *e) {
 if(e->eType==frmOpenEvent) {
  FormType *f=FrmGetActiveForm(); displayedConversation=gConversation.id; FrmSetTitle(f,gConversation.title);
  showing=gMessageCount?gMessageCount-1:0; showMessage(); setText(FIELD_INPUT,gConversation.draft);
  FrmDrawForm(f); AppStatus("Ready"); FrmSetFocus(f,FrmGetObjectIndex(f,FIELD_INPUT)); return true;
 }
 if(e->eType==frmCloseEvent) { saveDraft(); return false; }
 if(e->eType==fldChangedEvent && e->data.fldChanged.fieldID==FIELD_INPUT) { saveDraft(); return false; }
 if(e->eType==keyDownEvent && (e->data.keyDown.chr==vchrPageUp || e->data.keyDown.chr==vchrPageDown)) {
  FldScrollField(field(FIELD_REPLY),6,e->data.keyDown.chr==vchrPageUp?winUp:winDown); return true;
 }
 if(e->eType==ctlSelectEvent) {
  UInt16 id=e->data.ctlSelect.controlID;
  if(id==BUTTON_SEND) { sendQuestion(); return true; }
  if(NetBusy()) { AppAlert("Cancel the active request first."); return true; }
  if(id==BUTTON_NEW) { newChat(); return true; }
  if(id==BUTTON_CHATS) { saveDraft(); FrmGotoForm(FORM_LIST); return true; }
  if(id==BUTTON_PREV||id==BUTTON_NEXT) {
   if(id==BUTTON_PREV && showing) --showing;
   if(id==BUTTON_NEXT && showing+1<gMessageCount) ++showing;
   showMessage(); FldDrawField(field(FIELD_REPLY)); return true;
  }
 }
 return false;
}
static Boolean settingsEvent(EventType *e) {
 FormType *f=FrmGetActiveForm();
 if(e->eType==frmOpenEvent) {
  clearKey=false;
  setText(FIELD_URL,gPrefs.url); setText(FIELD_MODEL,gPrefs.model); setText(FIELD_KEY,"");
  CtlSetValue(FrmGetObjectPtr(f,FrmGetObjectIndex(f,CHECK_KEY)),gPrefs.rememberKey);
  FrmDrawForm(f); FrmSetFocus(f,FrmGetObjectIndex(f,FIELD_URL)); return true;
 }
 if(e->eType==ctlSelectEvent) {
  UInt16 id=e->data.ctlSelect.controlID;
  if(id==BUTTON_CLEAR_KEY) { clearKey=true; setText(FIELD_KEY,""); FldDrawField(field(FIELD_KEY)); AppAlert("API key will be cleared when you save."); return true; }
  if(id==BUTTON_CANCEL) { FrmGotoForm(FORM_CHAT); return true; }
  if(id==BUTTON_SAVE) {
   MossURL url; Char key[192];
   if(!moss_url(text(FIELD_URL),&url)) { AppAlert("Invalid Base URL. Use http://host:port/v1 or https://host/v1."); return true; }
   if(!*text(FIELD_MODEL)) { AppAlert("Enter a model ID."); return true; }
   StrCopy(gPrefs.url,text(FIELD_URL)); StrCopy(gPrefs.model,text(FIELD_MODEL));
   if(clearKey) gPrefs.key[0]=0;
   if(*text(FIELD_KEY)) StrCopy(gPrefs.key,text(FIELD_KEY));
   gPrefs.rememberKey=CtlGetValue(FrmGetObjectPtr(f,FrmGetObjectIndex(f,CHECK_KEY)))!=0;
   StrCopy(key,gPrefs.key); if(!gPrefs.rememberKey) gPrefs.key[0]=0;
   PrefSetAppPreferences(MOSS_CREATOR,0,1,&gPrefs,sizeof(gPrefs),true);
   StrCopy(gPrefs.key,key); MemSet(key,sizeof(key),0);
   FrmGotoForm(FORM_CHAT); return true;
  }
 }
 return false;
}
static Boolean listEvent(EventType *e) {
 FormType *f=FrmGetActiveForm(); ListType *list=FrmGetObjectPtr(f,FrmGetObjectIndex(f,LIST_CHATS));
 if(e->eType==frmOpenEvent) {
  UInt16 i; chatCount=StoreList(titles,chatRecords);
  for(i=0;i<chatCount;++i) titlePointers[i]=titles[i];
  LstSetListChoices(list,titlePointers,chatCount); LstSetSelection(list,0); FrmDrawForm(f); return true;
 }
 if(e->eType==ctlSelectEvent) {
  UInt16 id=e->data.ctlSelect.controlID;
  if(id==BUTTON_BACK) { FrmGotoForm(FORM_CHAT); return true; }
  if(id==BUTTON_NEW) { newChat(); return true; }
  if(id==BUTTON_OPEN) {
   Int16 selected=LstGetSelection(list);
   if(selected>=0 && selected<chatCount && StoreLoad(chatRecords[selected])) FrmGotoForm(FORM_CHAT);
   return true;
  }
 }
 return false;
}
static Boolean appEvent(EventType *e) {
 if(e->eType==frmLoadEvent) {
  FormType *f=FrmInitForm(e->data.frmLoad.formID);
  if(!f) { AppAlert("Not enough memory to open this screen."); return true; }
  FrmSetActiveForm(f);
  if(e->data.frmLoad.formID==FORM_CHAT) FrmSetEventHandler(f,chatEvent);
  else if(e->data.frmLoad.formID==FORM_SETTINGS) FrmSetEventHandler(f,settingsEvent);
  else if(e->data.frmLoad.formID==FORM_LIST) FrmSetEventHandler(f,listEvent);
  return true;
 }
 if(e->eType==menuEvent) return menu(e->data.menu.itemID);
 return false;
}
UInt32 PilotMain(UInt16 cmd,MemPtr cmdPBP,UInt16 launchFlags) {
 UInt32 rom; EventType event; UInt16 error;
 (void)cmdPBP; (void)launchFlags;
 if(cmd!=sysAppLaunchCmdNormalLaunch) return errNone;
 FtrGet(sysFtrCreator,sysFtrNumROMVersion,&rom);
 if(rom<sysMakeROMVersion(3,1,0,sysROMStageRelease,0)) { FrmAlert(ALERT_ROM); return sysErrRomIncompatible; }
 if(!StoreOpen()) { AppAlert("Cannot open Moss databases. Check free storage."); StoreClose(); return 1; }
 FrmGotoForm(FORM_CHAT);
 while(!stopping) {
  EvtGetEvent(&event,NetBusy()?2:evtWaitForever);
  if(event.eType==appStopEvent) { NetCancel(); saveDraft(); stopping=true; continue; }
  if(!SysHandleEvent(&event) && !MenuHandleEvent(NULL,&event,&error) && !appEvent(&event)) FrmDispatchEvent(&event);
  NetPump();
 }
 FrmCloseAllForms(); StoreClose(); return errNone;
}
