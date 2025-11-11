#include "libgraph.h"

#include "gs/gs_server_c.h"

void sceGsSetDefDBuff(sceGsDBuff* dp, short psm, short w, short h, short ztest, short zpsm, short clear)
{
}

void sceGsResetPath()
{
}

void sceGsResetGraph(short mode, short inter, short omode, short ffmode)
{
}

void sceGsPutDispEnv(sceGsDispEnv* disp)
{
}

int sceGsPutDrawEnv(sceGifTag* giftag)
{
}

void sceGsSetHalfOffset(sceGsDrawEnv1* draw, short centerx, short centery, short halfoff)
{
}

int sceGsSyncV(int mode)
{
    return 1;
}

int sceGsSwapDBuff(sceGsDBuff* db, int id)
{
}

int sceGsSetDefStoreImage(sceGsStoreImage* sp, short sbp, short sbw, short spsm, short x, short y, short w, short h)
{
}

int sceGsExecStoreImage(sceGsStoreImage* sp, u_long128* dstaddr)
{
}

int sceGsSyncPath(int mode, u_short timeout)
{
}

int sceGsSetDefLoadImage(sceGsLoadImage* lp, short dbp, short dbw, short dpsm, short x, short y, short w, short h)
{
    /// Setting up the BITBLTBUF tag
    lp->bitbltbuf.SBP = 0;
    lp->bitbltbuf.SBW = 0;
    lp->bitbltbuf.SPSM = 0;
    lp->bitbltbuf.DBP = dbp;
    lp->bitbltbuf.DBW = dbw;
    lp->bitbltbuf.DPSM = dpsm;
    lp->bitbltbufaddr = SCE_GS_BITBLTBUF;

    /// Setting up the TRXPOS tag
    lp->trxpos.DIR = 0;
    lp->trxpos.DSAX = x;
    lp->trxpos.DSAY = y;
    lp->trxpos.SSAX = 0;
    lp->trxpos.SSAY = 0;
    lp->trxdiraddr = SCE_GS_TRXPOS;

    /// Setting up the TRXREG tag
    lp->trxreg.RRW = w;
    lp->trxreg.RRH = h;
    lp->trxregaddr = SCE_GS_TRXREG;

    /// Setting up the TRXDIR tag
    lp->trxdir.XDR = 0;
    lp->trxdiraddr = SCE_GS_TRXDIR;
}

int sceGsExecLoadImage(sceGsLoadImage* lp, u_long128* srcaddr)
{
    GsUpload(lp, (unsigned char*)srcaddr);

    sceGsTex0 tex0;

    tex0.TBP0 = lp->bitbltbuf.DBP;
    tex0.TBW = lp->bitbltbuf.DBW;
    tex0.PSM = lp->bitbltbuf.DPSM;
    tex0.CPSM = lp->bitbltbuf.SPSM;
    tex0.CSA = 0;
    tex0.TW = lp->trxreg.RRW;
    tex0.TH = lp->trxreg.RRH;
}

int sceGsSetDefDrawEnv(sceGsDrawEnv1* draw, short psm, short w, short h, short ztest, short zpsm)
{
}

sceGsGParam* sceGsGetGParam()
{
}
