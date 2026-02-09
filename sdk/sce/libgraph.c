#include "libgraph.h"

#include "graphics/graph2d/sprt.h"
#include "mikupan/gs/gs_server_c.h"

#include <stddef.h>
#include <stdlib.h>

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
    return 1;
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
    return 1;
}

int sceGsSetDefStoreImage(sceGsStoreImage* sp, short sbp, short sbw, short spsm, short x, short y, short w, short h)
{
    /// Setting up the BITBLTBUF tag
    sp->bitbltbuf.SBP = 0;
    sp->bitbltbuf.SBW = 0;
    sp->bitbltbuf.SPSM = 0;
    sp->bitbltbuf.SBP = sbp;
    sp->bitbltbuf.DBP = sbp;
    sp->bitbltbuf.SBW = sbw;
    sp->bitbltbuf.DBW = sbw;
    sp->bitbltbuf.SPSM = spsm;
    sp->bitbltbuf.DPSM = spsm;
    sp->bitbltbufaddr = SCE_GS_BITBLTBUF;

    /// Setting up the TRXPOS tag
    sp->trxpos.DIR = 0;
    sp->trxpos.DSAX = x;
    sp->trxpos.DSAY = y;
    sp->trxpos.SSAX = 0;
    sp->trxpos.SSAY = 0;
    sp->trxdiraddr = SCE_GS_TRXPOS;

    /// Setting up the TRXREG tag
    sp->trxreg.RRW = w;
    sp->trxreg.RRH = h;
    sp->trxregaddr = SCE_GS_TRXREG;

    /// Setting up the TRXDIR tag
    sp->trxdir.XDR = 0;
    sp->trxdiraddr = SCE_GS_TRXDIR;

    return 1;
}

int sceGsExecStoreImage(sceGsStoreImage* sp, u_long128* dstaddr)
{
    //GsUpload((sceGsLoadImage*)&sp->giftag, (unsigned char *)dstaddr);
    return 1;
}

int sceGsSyncPath(int mode, u_short timeout)
{
    return 1;
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

    return 1;
}

int sceGsExecLoadImage(sceGsLoadImage* lp, u_long128* srcaddr)
{
    GsUpload(lp, (unsigned char*)srcaddr);
    return 1;
}

int sceGsSetDefDrawEnv(sceGsDrawEnv1* draw, short psm, short w, short h, short ztest, short zpsm)
{
    return 1;
}

sceGsGParam* gs_param = NULL;
sceGsGParam* sceGsGetGParam()
{
    if (gs_param == NULL)
    {
        gs_param = (sceGsGParam*)malloc(sizeof(sceGsGParam));
    }

    return gs_param;
}
