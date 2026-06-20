#ifndef TYPEDEFS_H 
#define TYPEDEFS_H

#ifdef _WIN32
#define __u_char_defined
#else
#include <sys/types.h>
#endif
#include <stdint.h>

#ifdef _MSC_VER
#define ATTRIBUTE_ALIGNED(x, decl) __declspec(align(x)) decl
#define TYPEDEF_ALIGNED(x, decl) typedef __declspec(align(x)) decl
#define MIKUPAN_PACKED_STRUCT_BEGIN __pragma(pack(push, 1))
#define MIKUPAN_PACKED_STRUCT_END __pragma(pack(pop))
#define MIKUPAN_PACKED
#define MIKUPAN_OPTIMIZE_O3
#include <math.h>
#include <stdlib.h>
#define __builtin_abs abs
#define __builtin_fabs fabs
#define __builtin_fabsf fabsf
#define __builtin_sqrtf sqrtf
#else
#define ATTRIBUTE_ALIGNED(x, decl) decl __attribute__((aligned(x)))
#define TYPEDEF_ALIGNED(x, decl) typedef decl __attribute__((aligned(x)))
#define MIKUPAN_PACKED_STRUCT_BEGIN
#define MIKUPAN_PACKED_STRUCT_END
#define MIKUPAN_PACKED __attribute__((packed))
#define MIKUPAN_OPTIMIZE_O3 __attribute__((optimize("O3")))
#endif

typedef long long s_long;
typedef short s_short;
typedef char s_char;
typedef int s_int;

/// TODO: CHECK FOR PROPER IMPLEMENTATION OF MODE TI
//typedef int s_long128 __attribute__ ((mode (TI)));
typedef int s_long128[4];
#ifdef _MSC_VER
TYPEDEF_ALIGNED(16, struct {
	uint64_t lo;
	uint64_t hi;
} u_long128);
#else
typedef unsigned int u_long128 __attribute__ ((mode (TI)));
#endif

static inline u_long128 u_long128_from_u64(uint64_t value)
{
#ifdef _MSC_VER
	u_long128 result = { value, 0 };
	return result;
#else
	return (u_long128)value;
#endif
}

typedef int (*sceSdTransIntrHandler)(int, void *);
typedef int (*sceSdSpu2IntrHandler)(int, void *);

#ifdef _WIN32
typedef unsigned long long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef unsigned int u_int;
#endif

typedef long long dword[2] ;
TYPEDEF_ALIGNED(16, int qword[4]);
TYPEDEF_ALIGNED(16, int sceVu0IVECTOR[4]);
TYPEDEF_ALIGNED(16, float sceVu0FVECTOR[4]);
TYPEDEF_ALIGNED(16, float sceVu0FMATRIX[4][4]);

typedef char u_long128_must_be_16_bytes[(sizeof(u_long128) == 16) ? 1 : -1];

typedef union {
	int ui32;
	float fl32;
} U32DATA;

typedef union {
	u_long128 ul128;
	u_long ul64[2];
	u_int ui32[4];
	float fl32[4];
	u_short us16[8];
	u_char uc8[16];
	sceVu0FVECTOR fv;
	sceVu0IVECTOR iv;
} Q_WORDDATA;

typedef union
{
    u_char *pu8;
    u_short *pu16;
    u_int *pu32;
    u_long *pu64;
    s_char *ps8;
    s_short *ps16;
    int *ps32;
    s_long *ps64;
    int64_t wrk;
} P_INT;

#endif /* TYPEDEFS_H */
