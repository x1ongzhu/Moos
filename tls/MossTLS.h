#ifndef MOSS_TLS_H
#define MOSS_TLS_H
#include <PalmOS.h>
#define MOSS_TLS_NAME "Moss TLS Library"
#define MOSS_TLS_CREATOR 'MTLS'
#define mtErrMemory 0x7d01
#define mtErrLibrary 0x7d02
#define mtErrEntropy 0x7d03
#define mtErrHandshake 0x7d04
#define mtErrIO 0x7d05
#define mtErrBusy 0x7d06
#define mtErrParam 0x7d07
#define MT_STATE_CLOSED 1
#define MT_STATE_SENDREC 2
#define MT_STATE_RECVREC 4
#define MT_STATE_READY 8
#define MT_STATE_DATA 16
#define MT_STATE_RECONNECT 32
#define MT_STATE_INSECURE 64
#ifdef MOSS_TLS_IMPLEMENTATION
#define MT_TRAP(n)
#else
#define MT_TRAP(n) SYS_TRAP(n)
#endif
Err MossTLSOpen(UInt16 ref) MT_TRAP(sysLibTrapOpen);
Err MossTLSClose(UInt16 ref) MT_TRAP(sysLibTrapClose);
Err MossTLSSleep(UInt16 ref) MT_TRAP(sysLibTrapSleep);
Err MossTLSWake(UInt16 ref) MT_TRAP(sysLibTrapWake);
Err MossTLSVersion(UInt16 ref,UInt32 *version) MT_TRAP(sysLibTrapCustom);
/* Caller owns Net Library and the connected TCP socket. Library validates host,
 * CA chain and device clock before releasing any application plaintext. */
Err MossTLSCreate(UInt16 ref,UInt16 netRef,Int16 socket,const Char *host,void **session) MT_TRAP(sysLibTrapCustom+1);
Err MossTLSPump(UInt16 ref,void *session,UInt16 *state,UInt16 *detail) MT_TRAP(sysLibTrapCustom+2);
Err MossTLSWrite(UInt16 ref,void *session,const void *data,UInt16 count,UInt16 *written) MT_TRAP(sysLibTrapCustom+3);
Err MossTLSRead(UInt16 ref,void *session,void *data,UInt16 capacity,UInt16 *received) MT_TRAP(sysLibTrapCustom+4);
Err MossTLSDestroy(UInt16 ref,void *session) MT_TRAP(sysLibTrapCustom+5);
typedef struct { UInt32 sent, received, workTicks; UInt16 state, lastIOError; } MossTLSStats;
Err MossTLSGetStats(UInt16 ref,void *session,MossTLSStats *stats) MT_TRAP(sysLibTrapCustom+7);
Err MossTLSLastX509Error(UInt16 ref,void *session,UInt32 *error) MT_TRAP(sysLibTrapCustom+6);
#endif
