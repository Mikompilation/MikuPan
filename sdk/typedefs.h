#ifndef TYPEDEFS_H 
#define TYPEDEFS_H

#ifdef _MSC_VER
#define ATTRIBUTE_ALIGNED(x, decl) __declspec(align(x)) decl
#else
#define ATTRIBUTE_ALIGNED(x, decl) decl __attribute__((aligned(x)))
#endif


#define __u_char_defined

typedef long s_long;
typedef short s_short;
typedef char s_char;
typedef int s_int;

/// TODO: CHECK FOR PROPER IMPLEMENTATION OF MODE TI
//typedef int s_long128 __attribute__ ((mode (TI)));
typedef int s_long128[4];

/// TODO: CHECK FOR PROPER IMPLEMENTATION OF MODE TI
typedef unsigned int u_long128 __attribute__ ((mode (TI)));

typedef int (*sceSdTransIntrHandler)(int, void *);
typedef int (*sceSdSpu2IntrHandler)(int, void *);

typedef unsigned long long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef unsigned int u_int;

typedef long long int dword[2] ;
ATTRIBUTE_ALIGNED(16, typedef int qword[4]);
ATTRIBUTE_ALIGNED(16, typedef int sceVu0IVECTOR[4]);
ATTRIBUTE_ALIGNED(16, typedef float sceVu0FVECTOR[4]);
ATTRIBUTE_ALIGNED(16, typedef float sceVu0FMATRIX[4][4]);

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

#endif /* TYPEDEFS_H */
