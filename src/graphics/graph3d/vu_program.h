#ifndef MIKUPAN_VU_PROGRAM_H
#define MIKUPAN_VU_PROGRAM_H
#include "sglib.h"

extern unsigned int dma /* __attribute__((section(".vutext"))) */;
extern void DRAWTYPE2() /* __attribute__((section(".vutext"))) */;
extern void DRAWTYPE2W();
extern void DRAWTYPE0();
extern void MULTI_PROLOGUE();

extern SgSourceChainTag SgSu_dma_start();
extern SgSourceChainTag SgSuP0_dma_start();
extern SgSourceChainTag SgSuP2_dma_start();
extern SgSourceChainTag SgSu_dma_starts();
extern void DIVP0_PROLOGUE_and_DIVP2_PROLOGUE();
extern void MULTIP_PROLOGUE();
extern void DP0_PROLOGUE();
extern void DP2_PROLOGUE();

extern void SHADOWDRAWTYPE0();
extern void SHADOWDRAWTYPE2();
extern void DIVPS_PROLOGUE();
extern void DPS_PROLOGUE();


#endif //MIKUPAN_VU_PROGRAM_H