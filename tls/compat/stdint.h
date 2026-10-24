#ifndef MOSS_STDINT_H
#define MOSS_STDINT_H
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned long uint32_t;
typedef signed long int32_t;
typedef unsigned long long uint64_t;
typedef signed long long int64_t;
typedef unsigned long uintptr_t;
#define UINT32_C(x) x##UL
#define UINT64_C(x) x##ULL
#define INT32_C(x) x##L
#define INT64_C(x) x##LL
#define UINT32_MAX 0xffffffffUL
#define UINT64_MAX 0xffffffffffffffffULL
#endif
