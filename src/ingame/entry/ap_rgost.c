#include "common.h"
#include "ap_rgost.h"
#include "rgst_dat.h"

RGOST_DSP_WRK rg_dsp_wrk[3];
RGOST_AP_DAT rg_ap_dat[];
RGOST_DAT rg_dat[];
sceVu0FVECTOR *rgc_dat[];
RG_DISP_DAT rg_start_dat[];
RG_DISP_DAT rg_inter_dat[];
RG_DISP_DAT rg_end_dat[];
RG_ALP_DAT *rg_alp_start[];
RG_ALP_DAT *rg_alp_inter[];
RG_ALP_DAT *rg_alp_end[];

void RareGhostInit()
{
}

void RareGhostEntrySet()
{
}

int RareGhostLoadReq()
{
}

int RareGhostLoadGameLoadReq()
{
}

void RareGhostMain()
{
}

void RareGhostAppearCtrl()
{
}

void RareGhostDispCtrl()
{
}

int RareGhostDispStartJudge(int rg_no)
{
}

int RareGhostDispEndJudge(int wrk_no)
{
}

void RareGhostDispTimeCtrl(int wrk_no, int rg_no)
{
}

void RareGhostDispEndSet(int wrk_no)
{
}

void RareGhostDispWrkSet(int wrk_no, int rg_no)
{
}

void SetRareGhostDispAlpha(int wrk_no, int rg_no)
{
}

void SetRareGhostDispAnimation(int wrk_no, int rg_no)
{
}

float SetRareGhostDispRate(int wrk_no)
{
}
