#ifndef TEST_PALMOS_H
#define TEST_PALMOS_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef int16_t Int16;
typedef uint32_t UInt32;
typedef int32_t Int32;
typedef int Boolean;
typedef int Err;
typedef char Char;
#define true 1
#define false 0
#define errNone 0
#define SYS_TRAP(n)
void *TestAlloc(size_t);
#define MemPtrNew TestAlloc
#define MemPtrFree free
#define MemMove memmove
#define MemSet(p,n,v) memset(p,v,n)
#define StrCopy strcpy
#define StrNCopy strncpy
#define StrCat strcat
#define StrLen strlen
#define StrCompare strcmp
static void StrIToA(char *s,Int32 n) { snprintf(s,12,"%ld",(long)n); }
UInt32 TimGetTicks(void);
UInt16 SysTicksPerSecond(void);
void EvtResetAutoOffTimer(void);
Err SysLibFind(const char *,UInt16 *);
Err SysLibLoad(UInt32,UInt32,UInt16 *);
Err SysLibRemove(UInt16);
#endif
