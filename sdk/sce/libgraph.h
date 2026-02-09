#ifndef SCE_LIBGRAPH_H
#define SCE_LIBGRAPH_H

#include "ee/eestruct.h"
#include "ee/eeregs.h"

#define SCE_GS_NOINTERLACE 0
#define SCE_GS_INTERLACE   1

#define	SCE_GS_FIELD  0
#define	SCE_GS_FRAME  1

#define SCE_GS_NTSC 2
#define SCE_GS_PAL  3

typedef struct  {
  /// Repeat count (GS primitive data size)
  ///     PACKED mode NREG x NLOOP(qword)
  ///         REGLIST mode NREG x NLOOP(dword)
  ///             IMAGE mode NLOOP(qword)
  unsigned long long NLOOP : 15;

  /// Termination information (End Of Packet)
  ///     0 With following primitive
  ///     1 Without following primitive (End of GS packet)
  unsigned long long EOP : 1;
  unsigned long long pad16 : 16;
  unsigned long long id : 14;

  /// PRIM field enable
  ///     0 Ignores PRIM field
  ///     1 Outputs PRIM field value to PRIM register
  unsigned long long PRE : 1;

  /// Data to be set to the PRIM register of GS
  unsigned long long PRIM : 11;

  /// Data format
  ///     00  PACKED mode
  ///     01  REGLIST mode
  ///     10  IMAGE mode
  ///     11  Disable (Same operation with the IMAGE mode)
  unsigned long long FLG : 2;

  /// Register descriptor
  ///     Number of register descriptors in REGS field
  ///         When the value is 0, the number of descriptors is 16.
  unsigned long long NREG : 4;

  /// PRIM
  unsigned long long REGS0 : 4;

  /// RGBAQ
  unsigned long long REGS1 : 4;

  /// ST
  unsigned long long REGS2 : 4;

  /// UV
  unsigned long long REGS3 : 4;

  /// XYZF2
  unsigned long long REGS4 : 4;

  /// XYZ2
  unsigned long long REGS5 : 4;

  /// TEX0_1
  unsigned long long REGS6 : 4;

  /// TEX0_2
  unsigned long long REGS7 : 4;

  /// CLAMP_1
  unsigned long long REGS8 : 4;

  /// CLAMP_2
  unsigned long long REGS9 : 4;

  /// FOG
  unsigned long long REGS10 : 4;
  unsigned long long REGS11 : 4;

  /// XYZF3
  unsigned long long REGS12 : 4;

  /// XYZ3
  unsigned long long REGS13 : 4;

  /// A+D
  unsigned long long REGS14 : 4;

  /// NOP
  unsigned long long REGS15 : 4;
} sceGifTag;



typedef struct {
    sceGsFrame frame1;
    u_long frame1addr;
    sceGsZbuf zbuf1;
    long zbuf1addr;
    sceGsXyoffset xyoffset1;
    long xyoffset1addr;
    sceGsScissor scissor1;
    long scissor1addr;
    sceGsPrmodecont prmodecont;
    long prmodecontaddr;
    sceGsColclamp colclamp;
    long colclampaddr;
    sceGsDthe dthe;
    long dtheaddr;
    sceGsTest test1;
    long test1addr;
} sceGsDrawEnv1 __attribute__((aligned(16)));

typedef struct {
    sceGsTest testa;
    long testaaddr;
    sceGsPrim prim;
    long primaddr;
    sceGsRgbaq rgbaq;
    long rgbaqaddr;
    sceGsXyz xyz2a;
    long xyz2aaddr;
    sceGsXyz xyz2b;
    long xyz2baddr;
    sceGsTest testb;
    long testbaddr;
} sceGsClear __attribute__((aligned(16)));

typedef struct {
    tGS_PMODE pmode;
    tGS_SMODE2 smode2;
    tGS_DISPFB2 dispfb;
    tGS_DISPLAY2 display;
    tGS_BGCOLOR bgcolor;
} sceGsDispEnv;

typedef struct {
    sceGsDispEnv disp[2];
    sceGifTag giftag0;
    sceGsDrawEnv1 draw0;
    sceGsClear clear0;
    sceGifTag giftag1;
    sceGsDrawEnv1 draw1;
    sceGsClear clear1;
} sceGsDBuff;

typedef struct {
  unsigned long long SBP : 14; /// Source Buffer Pointer
  unsigned long long pad14 : 2;
  unsigned long long SBW : 6; /// Source Buffer Width
  unsigned long long pad22 : 2;
  unsigned long long SPSM : 6; /// Source Pixel Storage Mode
  unsigned long long pad30 : 2;
  unsigned long long DBP : 14; /// Destination Buffer Pointer (i.e., 0x100 * 256 = 0x10000 bytes into GS memory)
  unsigned long long pad46 : 2;
  unsigned long long DBW : 6; /// Destination Buffer Width in blocks (each block = 64 pixels)
  unsigned long long pad54 : 2;
  unsigned long long DPSM : 6; /// Destination Pixel Storage Mode (e.g., 0x0 = PSMCT32, 32-bit color)
  unsigned long long pad62 : 2;
} sceGsBitbltbuf;

typedef struct {
  unsigned long long SSAX : 11; /// Source X coordinate (in pixels) where the transfer begins. Range 0–2047.
  unsigned long long pad11 : 5;
  unsigned long long SSAY : 11; /// Source Y coordinate (in pixels) where the transfer begins. Range 0–2047.
  unsigned long long pad27 : 5;
  unsigned long long DSAX : 11; /// Destination X coordinate (in pixels) where the transfer begins. Range 0–2047.
  unsigned long long pad43 : 5;
  unsigned long long DSAY : 11; /// Destination Y coordinate (in pixels) where the transfer begins. Range 0–2047.
  unsigned long long DIR : 2; /// X-axis direction: 0 = left→right, 1 = right→left. Y-axis direction: 0 = top→bottom, 1 = bottom→top.
  unsigned long long pad61 : 3;
} sceGsTrxpos;

typedef struct {
  unsigned long long RRW : 12; /// Rectangle Region Width
  unsigned long long pad12 : 20;
  unsigned long long RRH : 12; /// Rectangle Region Height
  unsigned long long pad44 : 20;
} sceGsTrxreg;

typedef struct {
  unsigned long long XDR : 2;
  unsigned long long pad01 : 30;
  unsigned long long pad02 : 32;
} sceGsTrxdir;

typedef struct {
  sceGifTag giftag0;
  sceGsBitbltbuf bitbltbuf;
  unsigned long long bitbltbufaddr;
  sceGsTrxpos trxpos;
  unsigned long long trxposaddr;
  sceGsTrxreg trxreg;
  unsigned long long trxregaddr;
  sceGsTrxdir trxdir;
  unsigned long long trxdiraddr;
  sceGifTag giftag1;
} sceGsLoadImage;

typedef struct { // 0x10
  /* 0x0 */ short int sceGsInterMode;
  /* 0x2 */ short int sceGsOutMode;
  /* 0x4 */ short int sceGsFFMode;
  /* 0x6 */ short int sceGsVersion;
  /* 0x8 */ int (*sceGsVSCfunc)(int);
  /* 0xc */ int sceGsVSCid;
} sceGsGParam __attribute__((aligned(16)));

typedef struct {
  long unsigned long pad00;
} sceGsFinish;

typedef struct {
  u_int vifcode[4];
  sceGifTag giftag;
  sceGsBitbltbuf bitbltbuf;
  unsigned long long bitbltbufaddr;
  sceGsTrxpos trxpos;
  unsigned long long trxposaddr;
  sceGsTrxreg trxreg;
  unsigned long long trxregaddr;
  sceGsFinish finish;
  unsigned long long finishaddr;
  sceGsTrxdir trxdir;
  unsigned long long trxdiraddr;
} sceGsStoreImage __attribute__((aligned(16)));



void sceGsSetDefDBuff(sceGsDBuff *dp, short psm, short w, short h, short ztest, short zpsm,  short clear);
void sceGsResetPath();
void sceGsResetGraph(short mode, short inter, short omode, short ffmode);
void sceGsPutDispEnv(sceGsDispEnv *disp);
int sceGsPutDrawEnv(sceGifTag *giftag);
void sceGsSetHalfOffset(sceGsDrawEnv1 *draw, short centerx, short centery, short halfoff);
int sceGsSyncV(int mode);
int sceGsSwapDBuff(sceGsDBuff *db, int id);
int sceGsSetDefStoreImage(sceGsStoreImage *sp, short sbp, short sbw, short spsm, short x, short y, short w, short h);
int sceGsExecStoreImage(sceGsStoreImage *sp, u_long128 *dstaddr);
int sceGsSyncPath(int mode, u_short timeout);
int sceGsSetDefLoadImage(sceGsLoadImage *lp, short dbp, short dbw, short dpsm, short x, short y, short w, short h);
int sceGsExecLoadImage(sceGsLoadImage *lp, u_long128 *srcaddr);
int sceGsSetDefDrawEnv(sceGsDrawEnv1 *draw, short psm, short w, short h, short ztest, short zpsm);
sceGsGParam *sceGsGetGParam();

#endif // SCE_LIBGRAPH_H