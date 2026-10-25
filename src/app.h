#ifndef MOSS_APP_H
#define MOSS_APP_H
#include <PalmOS.h>
#include <System/NetMgr.h>
#include "protocol.h"
#include "../resources/ids.h"
#define MOSS_CREATOR 'Moss'
#define MOSS_MAX_CHATS 32
#define MOSS_MAX_MESSAGES 128
#define MOSS_NO_RECORD 0xffff
#define MSG_WAITING 1
#define MSG_OK 2
#define MSG_FAILED 3

typedef struct {
 UInt16 version;
 Char url[193], model[64], key[192];
 Boolean rememberKey;
} MossPrefs;
typedef struct {
 UInt32 id, modified;
 Char title[40], draft[MOSS_MESSAGE_MAX+1];
} MossConversation;
typedef struct {
 UInt32 conversation, created;
 UInt8 assistant, state;
 UInt16 reserved;
} MossMessageHeader;
extern MossPrefs gPrefs;
extern MossConversation gConversation;
extern UInt16 gConversationRecord, gMessages[MOSS_MAX_MESSAGES], gMessageCount;
Boolean StoreOpen(void);
void StoreClose(void);
Boolean StoreLoad(UInt16 record);
Boolean StoreNew(void);
Boolean StoreSaveConversation(void);
UInt16 StoreAdd(Boolean assistant, UInt8 state, const Char *text);
Boolean StoreState(UInt16 record, UInt8 state);
Boolean StoreRead(UInt16 record, MossMessageHeader *header, Char *out, UInt16 cap);
void StoreIndex(void);
Boolean StoreDelete(void);
UInt16 StoreList(Char titles[MOSS_MAX_CHATS][40], UInt16 *records);
void NetStart(UInt16 questionRecord);
void NetPump(void);
void NetCancel(void);
Boolean NetBusy(void);
Boolean NetConnected(void);
void AppStatus(const Char *text);
void AppRefresh(void);
void AppStream(const Char *answer);
void AppAlert(const Char *text);
#endif
