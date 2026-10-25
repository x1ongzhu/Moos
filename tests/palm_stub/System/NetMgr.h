#ifndef TEST_NETMGR_H
#define TEST_NETMGR_H
#include <PalmOS.h>
typedef Int16 NetSocketRef;
typedef UInt32 NetIPAddr;
typedef struct { UInt16 family,port; NetIPAddr addr; } NetSocketAddrINType;
typedef NetSocketAddrINType NetSocketAddrType;
typedef struct { void **addrListP; } NetHostInfoBufType;
typedef NetHostInfoBufType *NetHostInfoPtr;
#define netErrAlreadyOpen 1
#define netErrTimeout 2
#define netErrWouldBlock 3
#define netSocketAddrINET 2
#define netSocketTypeStream 1
#define NetHToNS(n) (n)
Err NetLibOpen(UInt16,UInt16 *);
Err NetLibClose(UInt16,Boolean);
Err NetLibSocketClose(UInt16,NetSocketRef,UInt32,Err *);
NetIPAddr NetLibAddrAToIN(UInt16,const char *);
NetHostInfoPtr NetLibGetHostByName(UInt16,const char *,NetHostInfoBufType *,UInt32,Err *);
NetSocketRef NetLibSocketOpen(UInt16,int,int,int,UInt32,Err *);
Int16 NetLibSocketConnect(UInt16,NetSocketRef,NetSocketAddrType *,UInt16,UInt32,Err *);
Int16 NetLibSend(UInt16,NetSocketRef,const void *,UInt16,int,void *,int,UInt32,Err *);
Int16 NetLibReceive(UInt16,NetSocketRef,void *,UInt16,int,void *,void *,UInt32,Err *);
#endif
