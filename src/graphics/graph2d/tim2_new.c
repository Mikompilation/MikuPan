#include "common.h"
#include "typedefs.h"
#include "tim2_new.h"

#include <stdint.h>
#include <stdio.h>

#include "common/memory_addresses.h"
#include "main/glob.h"
#include "graphics/graph2d/tim2.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/effect_sub.h"
#include "gs/gs_server_c.h"

u_int *tm2_end_pkt = NULL;

static Q_WORDDATA fnt_pkt[512];
static Q_WORDDATA g2d_end_pkt[2];
static Q_WORDDATA g2d_top_pkt[2];

#define UNCACHED(p) ((char *)((u_int)p | 0x20000000))

/**
 * Implemented GS Texture Upload
 * @param pkt_addr
 * @param tim2_addr
 * @param tbp
 * @return
 */
u_int* MakeTim2Direct2(u_int *pkt_addr, u_int *tim2_addr, int tbp)
{
    TIM2_PICTUREHEADER *tph;
    u_int *img_addr;
    u_int psm;
    u_int tbw;
    u_int qwc;
    u_int nloop;
    sceGsTex0 sgtx0;
    Q_WORDDATA *qw;
    TIM2_FILEHEADER *tfh;

    tfh = (TIM2_FILEHEADER *)tim2_addr;

    if (tfh->FormatId == 0)
    {
        // FormatID is 0 => 16-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + sizeof(TIM2_FILEHEADER));
    }
    else if (tfh->FormatId == 1)
    {
        // Format ID is 1 => 128-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + 0x80);
    }

    nloop = tph->ImageSize / 16;

    img_addr = (u_int *)((int64_t)tph + tph->HeaderSize);

    sgtx0 = *(sceGsTex0*)&tph->GsTex0;

    psm = sgtx0.PSM;
    tbw = sgtx0.TBW;

    if (tbp < 0)
    {
        tbp = sgtx0.TBP0;
    }

    qw = (Q_WORDDATA *)(pkt_addr + 4);

    sceGsLoadImage load_image;

    /// Setting up the BITBLTBUF tag
    load_image.bitbltbuf.SBP = 0;
    load_image.bitbltbuf.SBW = 0;
    load_image.bitbltbuf.SPSM = 0;
    load_image.bitbltbuf.DBP = tbp;
    load_image.bitbltbuf.DBW = tbw;
    load_image.bitbltbuf.DPSM = psm;
    load_image.bitbltbufaddr = SCE_GS_BITBLTBUF;

    /// Setting up the TRXPOS tag
    load_image.trxpos.DIR = 0;
    load_image.trxpos.DSAX = 0;
    load_image.trxpos.DSAY = 0;
    load_image.trxpos.SSAX = 0;
    load_image.trxpos.SSAY = 0;
    load_image.trxdiraddr = SCE_GS_TRXPOS;

    /// Setting up the TRXREG tag
    load_image.trxreg.RRW = tph->ImageWidth;
    load_image.trxreg.RRH = tph->ImageHeight;
    load_image.trxregaddr = SCE_GS_TRXREG;

    /// Setting up the TRXDIR tag
    load_image.trxdir.XDR = 0;
    load_image.trxdiraddr = SCE_GS_TRXDIR;

    qw = SetImageTransParam2(qw, tbp, tbw, psm, tph->ImageWidth, tph->ImageHeight);

    GsUpload(&load_image, (unsigned char*)img_addr);

#ifdef BUILD_EU_VERSION
    qw->ul64[0] = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 0);
#else
    qw->ul64[0] = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 1);
#endif
    qw->ul64[1] = SCE_GIF_PACKED_AD;
    qw++;

    qwc = (((int)qw - (int)pkt_addr) >> 4) - 1;

    qw->ui32[0] = nloop | DMAref;
    qw->ui32[1] = (u_int)img_addr;
    qw->ui32[2] = 0;
    qw->ui32[3] = nloop | DMAcall;
    qw++;

    pkt_addr[0] = qwc | DMAcnt;
    pkt_addr[1] = 0;
    pkt_addr[2] = 0;
    pkt_addr[3] = qwc | DMAcall;

    return (u_int *)qw;
}

/**
 * Implemented GS Texture Upload
 * @param pkt_addr
 * @param tim2_addr
 * @param cbp
 * @return
 */
u_int* MakeClutDirect2(u_int *pkt_addr, u_int *tim2_addr, int cbp)
{
    TIM2_PICTUREHEADER *tph;
    u_int *img_addr;
    u_int psm;
    u_int tbw;
    u_int qwc;
    u_int nloop;
    sceGsTex0 sgtx0;
    Q_WORDDATA *qw;
    TIM2_FILEHEADER *tfh;

    tfh = (TIM2_FILEHEADER *)tim2_addr;

    if (tfh->FormatId == 0)
    {
        // FormatID is 0 => 16-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + sizeof(TIM2_FILEHEADER));
    }
    else if (tfh->FormatId == 1)
    {
        // Format ID is 1 => 128-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + 0x80);
    }

    nloop = tph->ClutSize / 16;

    img_addr = (u_int *)((int64_t)tph + (tph->HeaderSize + tph->ImageSize));

    sgtx0 = *(sceGsTex0*)&tph->GsTex0;

    psm = 0;
    tbw = 1;

    if (cbp < 0)
    {
        cbp = sgtx0.CBP;
    }

    qw = (Q_WORDDATA *)(pkt_addr + 4);

    sceGsLoadImage load_image;

    /// Setting up the BITBLTBUF tag
    load_image.bitbltbuf.SBP = 0;
    load_image.bitbltbuf.SBW = 0;
    load_image.bitbltbuf.SPSM = 0;
    load_image.bitbltbuf.DBP = cbp;
    load_image.bitbltbuf.DBW = tbw;
    load_image.bitbltbuf.DPSM = psm;
    load_image.bitbltbufaddr = SCE_GS_BITBLTBUF;

    /// Setting up the TRXPOS tag
    load_image.trxpos.DIR = 0;
    load_image.trxpos.DSAX = 0;
    load_image.trxpos.DSAY = 0;
    load_image.trxpos.SSAX = 0;
    load_image.trxpos.SSAY = 0;
    load_image.trxdiraddr = SCE_GS_TRXPOS;

    /// Setting up the TRXDIR tag
    load_image.trxdir.XDR = 0;
    load_image.trxdiraddr = SCE_GS_TRXDIR;

    if (tph->ClutColors == 16)
    {
        qw = SetImageTransParam2(qw, cbp, tbw, psm, 8, 2);

        /// Setting up the TRXREG tag
        load_image.trxreg.RRW = 8;
        load_image.trxreg.RRH = 2;
        load_image.trxregaddr = SCE_GS_TRXREG;
    }
    else
    {
        int h = tph->ClutColors / 16;
        qw = SetImageTransParam2(qw, cbp, tbw, psm, 16, h);

        /// Setting up the TRXREG tag
        load_image.trxreg.RRW = 16;
        load_image.trxreg.RRH = h;
        load_image.trxregaddr = SCE_GS_TRXREG;
    }

    GsUpload(&load_image, (unsigned char*)img_addr);

#ifdef BUILD_EU_VERSION
    qw->ul64[0] = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 0);
#else
    qw->ul64[0] = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 1);
#endif
    qw->ul64[1] = SCE_GIF_PACKED_AD;
    qw++;

    qwc = (((int)qw - (int)pkt_addr) >> 4) - 1;

    qw->ui32[0] = nloop | DMAref;
    qw->ui32[1] = (u_int)img_addr;
    qw->ui32[2] = 0;
    qw->ui32[3] = nloop | DMAcall;
    qw++;

    pkt_addr[0] = qwc | DMAcnt;
    pkt_addr[1] = 0;
    pkt_addr[2] = 0;
    pkt_addr[3] = qwc | DMAcall;

    return (u_int *)qw;
}

u_int* MakeTim2ClutDirect2(u_int *pkt_addr, u_int *tim2, int tbp, int cbp)
{
    pkt_addr = MakeTim2Direct2(pkt_addr, tim2, tbp);
    pkt_addr = MakeClutDirect2(pkt_addr, tim2, cbp);

    return pkt_addr;
}

Q_WORDDATA* SetImageTransParam2(Q_WORDDATA *qw, int tbp, int tbw, int psm, u_short w, u_short h)
{
    qw->ul64[0] = SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);
    qw->ul64[1] = SCE_GIF_PACKED_AD;
    qw++;

    sceGsLoadImage load_image;

    /// Setting up the BITBLTBUF tag
    load_image.bitbltbuf.SBP = 0;
    load_image.bitbltbuf.SBW = 0;
    load_image.bitbltbuf.SPSM = 0;
    load_image.bitbltbuf.DBP = tbp;
    load_image.bitbltbuf.DBW = tbw;
    load_image.bitbltbuf.DPSM = psm;
    load_image.bitbltbufaddr = SCE_GS_BITBLTBUF;
    qw->ul64[0] = SCE_GS_SET_BITBLTBUF(0, 0, 0, tbp, tbw, psm);
    qw->ul64[1] = SCE_GS_BITBLTBUF;
    qw++;

    /// Setting up the TRXPOS tag
    load_image.trxpos.DIR = 0;
    load_image.trxpos.DSAX = 0;
    load_image.trxpos.DSAY = 0;
    load_image.trxpos.SSAX = 0;
    load_image.trxpos.SSAY = 0;
    load_image.trxdiraddr = SCE_GS_TRXPOS;
    qw->ul64[0] = SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0);
    qw->ul64[1] = SCE_GS_TRXPOS;
    qw++;

    /// Setting up the TRXREG tag
    load_image.trxreg.RRW = w;
    load_image.trxreg.RRH = h;
    load_image.trxregaddr = SCE_GS_TRXREG;
    qw->ul64[0] = SCE_GS_SET_TRXREG(w, h);
    qw->ul64[1] = SCE_GS_TRXREG;
    qw++;

    /// Setting up the TRXDIR tag
    load_image.trxdir.XDR = 0;
    load_image.trxdiraddr = SCE_GS_TRXDIR;
    qw->ul64[0] = SCE_GS_SET_TRXDIR(0);
    qw->ul64[1] = SCE_GS_TRXDIR;
    qw++;

    /// Setting up the giftag1 tag
    *(u_long*)&load_image.giftag0 = SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);

    return qw;
}

u_int* ChainPK2Direct(u_int *pkt_addr, u_int *tm2_addr)
{
    int i;
    int texnum;
    int *offtop;

    int64_t addr = (int64_t)tm2_addr;

    texnum = ((int *)addr)[0];
    offtop = &((int *)addr)[4];

    for (i = 0; i < texnum; i++)
    {
        pkt_addr = MakeTim2ClutDirect2(pkt_addr, (u_int *)(addr + offtop[i]), -1, -1);
    }

    return pkt_addr;
}

void MakeFontTexSendPacket()
{
    SPRITE_DATA dummy;

    /// When uninitialized the value will be 0, otherwise this value will be the
    /// value of a pk2 header which indicates the num of textures within the
    int64_t addr = FontTextAddress;

    if (addr == 0)
    {
        return;
    }

    int texnum = ((int*)addr)[0];
    int* offtop = &((int*)addr)[4];

    u_int* pkt_addr = (u_int*)fnt_pkt;

    for (int i = 0; i < texnum; i++)
    {
        LoadTIM2Sub(&dummy, (char *)(addr + offtop[i]), 0, 0);
    }

    tm2_end_pkt = ChainPK2Direct(pkt_addr, (u_int *)addr);
}

void CallFontTexSendPacket()
{
    tm2_end_pkt[0] = DMAend;
    tm2_end_pkt[1] = 0;
    tm2_end_pkt[2] = 0;
    tm2_end_pkt[3] = 0;

    sceGsSyncPath(0, 0);
    DrawOne2D_P2(fnt_pkt);
    sceGsSyncPath(0, 0);
}

void CallFontTexAndMesPacket()
{
    void *mmp;

    mmp = DrawAllMes_P2(0);

    if (mmp == NULL)
    {
        tm2_end_pkt[0] = DMAend;
        tm2_end_pkt[1] = 0;
        tm2_end_pkt[2] = 0;
        tm2_end_pkt[3] = 0;
    }
    else
    {
        tm2_end_pkt[0] = DMAnext;
        tm2_end_pkt[1] = (u_int)mmp;
        tm2_end_pkt[2] = VU0_ADDRESS;
        tm2_end_pkt[3] = 0;
    }

    DrawOne2D_P2(fnt_pkt);
}

/**
 * Uploads the Tim2 data to the GS
 * @param tim2_addr
 * @param tbp
 * @param offset
 */
void MakeTim2Direct3(u_int *tim2_addr, int tbp, int offset)
{
    TIM2_PICTUREHEADER *tph;
    u_int *img_addr;
    u_int psm;
    u_int tbw;
    u_int qwc;
    u_int nloop;
    sceGsTex0 sgtx0;
    int qwtop;
    TIM2_FILEHEADER *tfh;

    tfh = (TIM2_FILEHEADER *)tim2_addr;

    if (tfh->FormatId == 0)
    {
        // FormatID is 0 => 16-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + sizeof(TIM2_FILEHEADER));
    }
    else if (tfh->FormatId == 1)
    {
        // Format ID is 1 => 128-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + 0x80);
    }

    if (tph->ImageSize == 0)
    {
        return;
    }

    nloop = tph->ImageSize / 16;

    img_addr = (u_int *)((int64_t)tph + tph->HeaderSize);

    sgtx0 = *(sceGsTex0*)&tph->GsTex0;

    psm = sgtx0.PSM;
    tbw = sgtx0.TBW;

    if (tbp < 0)
    {
        tbp = sgtx0.TBP0 + offset;
    }

    sceGsLoadImage load_image;

    qwtop = ndpkt++;

    /// Setting up the giftag1 tag
    pbuf[ndpkt].ul64[0] = SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);
    pbuf[ndpkt++].ul64[1] = SCE_GIF_PACKED_AD;
    *(u_long*)&load_image.giftag0 = SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);

    /// Setting up the BITBLTBUF tag
    pbuf[ndpkt].ul64[0] = SCE_GS_SET_BITBLTBUF(0, 0, 0, tbp, tbw, psm);
    pbuf[ndpkt++].ul64[1] = SCE_GS_BITBLTBUF;
    load_image.bitbltbuf.SBP = 0;
    load_image.bitbltbuf.SBW = 0;
    load_image.bitbltbuf.SPSM = 0;
    load_image.bitbltbuf.DBP = tbp;
    load_image.bitbltbuf.DBW = tbw;
    load_image.bitbltbuf.DPSM = psm;
    load_image.bitbltbufaddr = SCE_GS_BITBLTBUF;

    /// Setting up the TRXPOS tag
    pbuf[ndpkt].ul64[0] = SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0);
    pbuf[ndpkt++].ul64[1] = SCE_GS_TRXPOS;
    load_image.trxpos.DIR = 0;
    load_image.trxpos.DSAX = 0;
    load_image.trxpos.DSAY = 0;
    load_image.trxpos.SSAX = 0;
    load_image.trxpos.SSAY = 0;
    load_image.trxdiraddr = SCE_GS_TRXPOS;

    /// Setting up the TRXREG tag
    pbuf[ndpkt].ul64[0] = SCE_GS_SET_TRXREG(tph->ImageWidth, tph->ImageHeight);
    pbuf[ndpkt++].ul64[1] = SCE_GS_TRXREG;
    load_image.trxreg.RRW = tph->ImageWidth;
    load_image.trxreg.RRH = tph->ImageHeight;
    load_image.trxregaddr = SCE_GS_TRXREG;

    /// Setting up the TRXDIR tag
    pbuf[ndpkt].ul64[0] = SCE_GS_SET_TRXDIR(0);
    pbuf[ndpkt++].ul64[1] = SCE_GS_TRXDIR;

    load_image.trxdir.XDR = 0;
    load_image.trxdiraddr = SCE_GS_TRXDIR;

    /// Setting up the giftag1 tag
    pbuf[ndpkt].ul64[0] = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 1);
    pbuf[ndpkt++].ul64[1] = SCE_GIF_PACKED_AD;
    *(u_long*)&load_image.giftag1 = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 1);

    GsUpload(&load_image, (unsigned char*)img_addr);

    qwc = (ndpkt - qwtop) - 1;

    pbuf[ndpkt].ui32[0] = nloop | DMAref;
    pbuf[ndpkt].ui32[1] = (int64_t)img_addr;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = nloop | DMAcall;

    pbuf[qwtop].ui32[0] = qwc | DMAcnt;
    pbuf[qwtop].ui32[1] = 0;
    pbuf[qwtop].ui32[2] = 0;
    pbuf[qwtop].ui32[3] = qwc | DMAcall;
}

void MakeClutDirect3(u_int *tim2_addr, int cbp, int offset)
{
    TIM2_PICTUREHEADER *tph;
    u_int *img_addr;
    u_int qwc;
    u_int nloop;
    sceGsTex0 sgtx0;
    int qwtop;
    TIM2_FILEHEADER *tfh;

    tfh = (TIM2_FILEHEADER *)tim2_addr;

    if (tfh->FormatId == 0)
    {
        // FormatID is 0 => 16-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + sizeof(TIM2_FILEHEADER));
    }
    else if (tfh->FormatId == 1)
    {
        // Format ID is 1 => 128-byte alignment
        tph = (TIM2_PICTUREHEADER *)((char *)tim2_addr + 0x80);
    }

    if (tph->ClutSize == 0)
    {
        return;
    }

    nloop = tph->ClutSize / 16;

    img_addr = (u_int *)((int64_t)tph + (tph->HeaderSize + tph->ImageSize));

    sgtx0 = *(sceGsTex0*)&tph->GsTex0;

    if (cbp < 0)
    {
        cbp = sgtx0.CBP + offset;
    }

    sceGsLoadImage load_image;
    qwtop = ndpkt++;

    /// Setting up the giftag1 tag
    pbuf[ndpkt].ul64[0] = SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);
    pbuf[ndpkt++].ul64[1] = SCE_GIF_PACKED_AD;
    *(u_long*)&load_image.giftag0 = SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);

    /// Setting up the BITBLTBUF tag
    pbuf[ndpkt].ul64[0] = SCE_GS_SET_BITBLTBUF(0, 0, SCE_GS_PSMCT32, cbp, 1, SCE_GS_PSMCT32);
    pbuf[ndpkt++].ul64[1] = SCE_GS_BITBLTBUF;
    load_image.bitbltbuf.SBP = 0;
    load_image.bitbltbuf.SBW = 0;
    load_image.bitbltbuf.SPSM = SCE_GS_PSMCT32;
    load_image.bitbltbuf.DBP = cbp;
    load_image.bitbltbuf.DBW = 1;
    load_image.bitbltbuf.DPSM = SCE_GS_PSMCT32;
    load_image.bitbltbufaddr = SCE_GS_BITBLTBUF;

    /// Setting up the TRXPO tag
    pbuf[ndpkt].ul64[0] = 0;
    pbuf[ndpkt++].ul64[1] = SCE_GS_TRXPOS;
    load_image.trxpos.DIR = 0;
    load_image.trxpos.DSAX = 0;
    load_image.trxpos.DSAY = 0;
    load_image.trxpos.SSAX = 0;
    load_image.trxpos.SSAY = 0;
    load_image.trxdiraddr = SCE_GS_TRXPOS;

    /// Setting up the TRXREG tag
    if (tph->ClutColors == 16)
    {
        pbuf[ndpkt].ul64[0] = SCE_GS_SET_TRXREG(8, 2);
        load_image.trxreg.RRW = 8;
        load_image.trxreg.RRH = 2;
    }
    else
    {
        int rrh = tph->ClutColors / 16;
        pbuf[ndpkt].ul64[0] = SCE_GS_SET_TRXREG(16, rrh);
        load_image.trxreg.RRW = 16;
        load_image.trxreg.RRH = rrh;
    }

    pbuf[ndpkt++].ul64[1] = SCE_GS_TRXREG;
    load_image.trxregaddr = SCE_GS_TRXREG;

    /// Setting up the TRXDIR tag
    pbuf[ndpkt].ul64[0] = SCE_GS_SET_TRXDIR(0);
    pbuf[ndpkt++].ul64[1] = SCE_GS_TRXDIR;
    load_image.trxdir.XDR = 0;
    load_image.trxdiraddr = SCE_GS_TRXDIR;

    /// Setting up the giftag1 tag
    pbuf[ndpkt].ul64[0] = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 1);
    pbuf[ndpkt++].ul64[1] = SCE_GIF_PACKED_AD;
    *(u_long*)&load_image.giftag1 = SCE_GIF_SET_TAG(nloop, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_IMAGE, 1);

    GsUpload(&load_image, (unsigned char*)img_addr);

    qwc = (ndpkt - qwtop) - 1;

    pbuf[ndpkt].ui32[0] = nloop | DMAref;
    pbuf[ndpkt].ui32[1] = (u_int)img_addr;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = nloop | DMAcall;

    pbuf[qwtop].ui32[0] = qwc | DMAcnt;
    pbuf[qwtop].ui32[1] = 0;
    pbuf[qwtop].ui32[2] = 0;
    pbuf[qwtop].ui32[3] = qwc | DMAcall;
}

/**
 * Generates DMA transfer tags for all Tim2 within PAK file
 * @param tm2_addr Address for the Tim2 texture
 * @param tbp Texture buffer pointer
 * @param cbp Clut buffer pointer
 * @param offset
 */
void MakeTim2ClutDirect3(int64_t tm2_addr, int tbp, int cbp, int offset)
{
    int i;
    int texnum;
    int *offtop;
    int64_t addr;

    Reserve2DPacket_Load();

    pbuf[ndpkt].ui32[0] = DMAcnt;
    pbuf[ndpkt].ui32[1] = 0;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;

    addr = tm2_addr;

    texnum = ((int *)addr)[0];
    offtop = &((int *)addr)[4];

    for (i = 0; i < texnum; i++)
    {
        MakeTim2Direct3((u_int *)(addr + offtop[i]), -1, offset);
        MakeClutDirect3((u_int *)(addr + offtop[i]), -1, offset);
    }

    pbuf[ndpkt].ui32[0] = DMAnext;
    pbuf[ndpkt].ui32[1] = (u_int)&pbuf[ndpkt+1] & 0x0fffffff;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;
}

void MakeTim2ClutDirect4(int64_t tm2_addr, int num, int tbp, int cbp, int offset)
{
    int i;
    int texnum;
    int *offtop;
    int64_t addr;

    Reserve2DPacket_Load();

    pbuf[ndpkt].ui32[0] = DMAcnt;
    pbuf[ndpkt].ui32[1] = 0;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;

    addr = (int)tm2_addr;

    texnum = ((int *)addr)[0];
    offtop = &((int *)addr)[4];

    for (i = 0; i < texnum; i++)
    {
        if (i == num)
        {
            MakeTim2Direct3((u_int *)(addr + offtop[i]), -1, offset);
            MakeClutDirect3((u_int *)(addr + offtop[i]), -1, offset);
        }
    }

    pbuf[ndpkt].ui32[0] = DMAnext;
    pbuf[ndpkt].ui32[1] = (u_int)&pbuf[ndpkt+1] & 0x0fffffff;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;
}

void MakeTim2SendPacket(int64_t tm2_addr, int offset)
{
    Reserve2DPacket_Load();

    pbuf[ndpkt].ui32[0] = DMAcnt;
    pbuf[ndpkt].ui32[1] = 0;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;

    MakeTim2Direct3((u_int *)tm2_addr, -1, offset);
    MakeClutDirect3((u_int *)tm2_addr, -1, offset);

    pbuf[ndpkt].ui32[0] = DMAnext;
    pbuf[ndpkt].ui32[1] = (u_int)&pbuf[ndpkt+1] & 0x0fffffff;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;
}

void MakeTim2SendPacket_3Dpkt(int64_t tm2_addr, int offset)
{
    AppendDmaTagCall((int64_t)&pbuf[ndpkt]);

    pbuf[ndpkt].ui32[0] = DMAcnt;
    pbuf[ndpkt].ui32[1] = 0;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;

    MakeTim2Direct3((u_int *)tm2_addr, -1, offset);
    MakeClutDirect3((u_int *)tm2_addr, -1, offset);

    pbuf[ndpkt].ui32[0] = DMAnext;
    pbuf[ndpkt].ui32[1] = 0;
    pbuf[ndpkt].ui32[2] = 0;
    pbuf[ndpkt++].ui32[3] = VU0_ADDRESS;

    AppendDmaTagNextRet(&ndpkt[pbuf-1]);
    SetG2DTopPkt((int64_t)&pbuf[ndpkt]);
    FlushModel(0);
}

void Reserve2DPacket(u_int pri)
{
    draw_pri[ndpri][0] = pri;
    draw_pri[ndpri++][1] = ndpkt;
}

void Reserve2DPacket_Load()
{
    draw_pri[ndpri][0] = 0x1000;
    draw_pri[ndpri++][1] = ndpkt;
}

void SetG2DTopPkt(int64_t addr)
{
    g2d_top_pkt[mes_swap].ui32[0] = DMAnext;
    g2d_top_pkt[mes_swap].ui32[1] = addr & 0xfffffff;
    g2d_top_pkt[mes_swap].ui32[2] = VU0_ADDRESS;
    g2d_top_pkt[mes_swap].ui32[3] = 0; // DMArefe ?
}

Q_WORDDATA* Get2DPacketBufferAddress()
{
    return &pbuf[ndpkt];
}

void Set2DPacketBufferAddress(Q_WORDDATA *addr)
{
    ndpkt = (((u_int)addr & 0xfffffff) - ((u_int)pbuf & 0xfffffff)) / 16;
}

void DrawAll2DMes_P2()
{
    int i;
    int m;
    int n;
    int s;
    void *mmp;
    int nch;
    int npk;
    int mch;
    int mpk;

    DrawPerformanceCounter();

    if (ndpri > 0)
    {
        for (i = 0; i < ndpri-1; i++)
        {
#ifdef MATCHING_DECOMP
            // hack to fix t0<->t1 regswap
            if (pbuf)
            {
                unsigned char x = -x;
            }
#endif

            n = draw_pri[i][1];
            m = draw_pri[i+1][1];

            s = pbuf[n].us16[0];

            if (pbuf[n].uc8[3] == 0x70) // upper part of 0x70000000 (DMAend) ??
            {
                pbuf[n].uc8[3] = 0x20; // upper part of 0x20000000 (DMAnext) ?? // Line 602
                pbuf[n].ui32[1] = (u_int)&pbuf[m] & 0x0fffffff;
                pbuf[n].ui32[2] = 0;

                if (s != 0)
                {
                    pbuf[n].ui32[3] = s | DMAcall;
                }
                else
                {
                    pbuf[n].ui32[3] = 0;
                }
            }
        }

        n = draw_pri[ndpri-1][1];

        s = pbuf[n].us16[0];

        if (pbuf[n].uc8[3] == 0x70) // upper part of 0x70000000 (DMAend) ??
        {
            pbuf[n].uc8[3] = 0x20; // upper part of 0x20000000 (DMAnext) ??
            pbuf[n].ui32[1] = (u_int)&pbuf[ndpkt] & 0x0fffffff;
            pbuf[n].ui32[2] = 0;

            if (s != 0)
            {
                pbuf[n].ui32[3] = s | DMAcall;
            }
            else
            {
                pbuf[n].ui32[3] = 0;
            }
        }
    }
    else
    {
        ndpkt = 0;
    }

    mch = nmdpri;
    mpk = nmdpkt;

    mmp = DrawAllMes_P2((int64_t)&g2d_end_pkt[mes_swap]);

    if (mmp == NULL)
    {
        pbuf[ndpkt].ui32[0] = DMAnext;
        pbuf[ndpkt].ui32[1] = (u_int)&g2d_end_pkt[mes_swap] & 0x0fffffff;
        pbuf[ndpkt].ui32[2] = 0;
        pbuf[ndpkt++].ui32[3] = 0;
    }
    else
    {
        pbuf[ndpkt].ui32[0] = DMAnext;
        pbuf[ndpkt].ui32[1] = (u_int)fnt_pkt & 0x0fffffff;
        pbuf[ndpkt].ui32[2] = 0;
        pbuf[ndpkt++].ui32[3] = 0;

        tm2_end_pkt[0] = DMAnext;
        tm2_end_pkt[1] = (u_int)mmp & 0x0fffffff;
        tm2_end_pkt[2] = VU0_ADDRESS;
        tm2_end_pkt[3] = 0;
    }

    g2d_end_pkt[mes_swap].ui32[0] = DMAnext;
    g2d_end_pkt[mes_swap].ui32[1] = 0;
    g2d_end_pkt[mes_swap].ui32[2] = VU0_ADDRESS;
    g2d_end_pkt[mes_swap].ui32[3] = 0;

    AppendDmaTagCall((int64_t)&g2d_top_pkt[mes_swap]);
    AppendDmaTagNextRet(&g2d_end_pkt[mes_swap]);

    nch = ndpri;
    npk = ndpkt;

    if (ndpkt > 0x3000)
    {
        printf("2D-PacketBuffer Half Over\n");
    }

    ndpri = 0;
    ndpkt = 0;

    mes_swap = mes_swap ^ 1;

    mpbuf = (Q_WORDDATA *)UNCACHED(mpbufw[mes_swap]);

    mmp = (void *)(mes_swap * 0x60000 + 0x720000); // what's this ??
    pbuf = (Q_WORDDATA *)UNCACHED(mmp);

    SetG2DTopPkt((int64_t)pbuf);

    if (dbg_wrk.oth_pkt_num_sw == 0)
    {
        SetPanel2(0x20, 44.0f, 360.0f, 580.0f, 396.0f, 0, 0x0, 0x0, 0x0, 0x80);
        SetString2(0x10, 48.0f, 364.0f, 1, 0x80, 0x80, 0x80, "2D Chain  Num : %4d", nch);
        SetString2(0x10, 48.0f, 380.0f, 1, 0x80, 0x80, 0x80, "2D Packet Num : %4d", npk);
        SetString2(0x10, 320.0f, 364.0f, 1, 0x80, 0x80, 0x80, "Mes Chain  Num : %4d", mch);
        SetString2(0x10, 320.0f, 380.0f, 1, 0x80, 0x80, 0x80, "Mes Packet Num : %4d", mpk);
    }
}

#ifdef BUILD_EU_VERSION
void SendFontTex()
{
    if (sys_wrk.language != 0)
    {
        tm2_end_pkt[0] = DMAend;
        tm2_end_pkt[1] = 0;
        tm2_end_pkt[2] = 0;
        tm2_end_pkt[3] = 0;

        DrawOne2D_P2(fnt_pkt);
    }
}
#endif
