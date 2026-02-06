#include "iopcdvd.h"

#include "../../mikupan/mikupan_logging_c.h"
#include "SDL3/SDL_mutex.h"
#include "enums.h"
#include "iop/adpcm/iopadpcm.h"
#include "iop/iopmain.h"
#include "iop/se/iopse.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_memory.h"
#include "os/eeiop/eeiop.h"

#include <stdio.h>
#include <stdlib.h>

CDVD_STAT cdvd_stat;
CDVD_REQ_BUF cdvd_req[32];
CDVD_LOAD_STAT load_stat[32];
CDVD_TRANS_STAT cdvd_trans[2];

// UNUSED: ToDo Refractor and ... cast pointer in here?
//u_int *load_buf_table[2];

static void ICdvdInitOnce();
static void ICdvdInitSoftReset();
static void ICdvdAddCmd(IOP_COMMAND *icp);
static void ICdvdTransSe(IOP_COMMAND *icp);
static void ICdvdTransSeInit();
static void ICdvdSetRetStat(int id, u_char stat);
static int ICdvdExecCmdSub(CDVD_REQ_BUF *req_bufp);
static int ICdvdCheckBufStatus();
static int ICdvdSeTransCB(int channel, void *data);
static void ICdvdTransCheck();
static void ICdvdAdpcmLoad();
static void ICdvdExecCmd();
static void ICdvdDataReadOnceSector(CDVD_REQ_BUF *rq);
static int ICdvdCheckLoadError();
static void ICdvdTransFinishedData();

// --- ALL functions have been heavily modified for PC ---

void ICdvdCmd(IOP_COMMAND *icp)
{
    switch (icp->cmd_no)
    {
        case IC_CDVD_INIT:
            ICdvdInit(icp->data1);
            break;
        case IC_CDVD_LOAD:
        case IC_CDVD_LOAD_SECT:
        case IC_CDVD_SEEK:
            ICdvdAddCmd(icp);
            break;
        case IC_CDVD_SE_TRANS:
            ICdvdTransSe(icp);
            break;
        case IC_CDVD_SE_TRANS_RESET:
            ICdvdTransSeInit();
            break;
        case IC_CDVD_BREAK:
            ICdvdBreak();
            break;
    }
}

void ICdvdInit(int reset)
{
    if (reset)
        ICdvdInitSoftReset();
    else
        ICdvdInitOnce();
}

void ICdvdInitOnce()
{
    memset(&cdvd_stat, 0, sizeof(cdvd_stat));
    memset(cdvd_req, 0, sizeof(cdvd_req));
    memset(&iop_stat.cdvd, 0, sizeof(iop_stat.cdvd));
    memset(cdvd_trans, 0, sizeof(cdvd_trans));
    //load_buf_table[0] = malloc(0x64000);

    /*if (load_buf_table[0])
    {
        load_buf_table[1] = load_buf_table[0] + 0x32000;
        iop_stat.cdvd.ld_addr = load_buf_table[0];
    }*/

    cdvd_stat.fp = fopen("\\IMG.BD.BIN", "r");
    cdvd_stat.lock = SDL_CreateMutex();

    MikuPan_LoadImgHdFile();
}

void ICdvdInitSoftReset()
{
    memset(&cdvd_stat, 0, sizeof(cdvd_stat));
    memset(cdvd_req, 0, sizeof(cdvd_req));
    memset(&iop_stat.cdvd, 0, sizeof(iop_stat.cdvd));
}

void ICdvdDoTransfer(CDVD_REQ_BUF *rq)
{
    int sizeRead;
    switch (rq->tmem)
    {
        case TRANS_MEM_EE:// EE
            MikuPan_ReadFileInArchive(rq->start_sector, rq->size_sector, rq->taddr);
            break;
        case TRANS_MEM_IOP:// IOP
            info_log("CDVD transfer to IOP unimplemented");
            break;
        case TRANS_MEM_SPU:// SPU
            break;
    }

    cdvd_stat.now_size = sizeRead - cdvd_stat.end_size;
}

static void ICdvdAdpcmLoad()
{
    u_char pos;

    pos = 0xff;
    if (cdvd_stat.adpcm[0].req_type >= cdvd_stat.adpcm[1].req_type)
    {
        pos = 0;
    }
    else
    {
        pos = 1;
    }

    MikuPan_ReadFileInArchive64(cdvd_stat.adpcm[pos].start, cdvd_stat.adpcm[pos].size_now, &AdpcmIopBuf[pos]);

    cdvd_stat.adpcm[0].now_load = 1;
    cdvd_stat.adpcm_req = 1;
}

static void ICdvdDataReadOnceSector(CDVD_REQ_BUF *rq)
{
    if (cdvd_stat.now_size > 204800)
    {
        cdvd_stat.now_size = 204800;
        cdvd_trans[cdvd_stat.now_lbuf].ltrans = 0;
    }
    else
    {
        cdvd_trans[cdvd_stat.now_lbuf].ltrans = 1;
    }

    ICdvdDoTransfer(rq);
}

void ICdvdMain()
{
    CDVD_REQ_BUF rq;
    bool found_work = false;

    SDL_LockMutex(cdvd_stat.lock);

    ICdvdTransCheck();

    if (cdvd_stat.buf_use_num)
    {
        rq = cdvd_req[cdvd_stat.start_pos];
        cdvd_stat.cmd_on = 1;
    }

    SDL_UnlockMutex(cdvd_stat.lock);

    if (cdvd_stat.stat == CDVD_STAT_FREE)
    {
        if (cdvd_stat.adpcm_req)
        {

            ICdvdAdpcmLoad();
            cdvd_stat.stat = CDVD_STAT_STREAMING;
        }
        else
        {
            if (cdvd_stat.cmd_on)
            {
                ICdvdDataReadOnceSector(&rq);
                cdvd_stat.stat = CDVD_STAT_LOADING;
            }
            else
            {

                ICdvdExecCmd();
            }
        }
        return;
    }

    if (cdvd_stat.stat == CDVD_STAT_LOADING)
    {
        //if (!sceCdSync(1))
        {
            int error_stat;

            cdvd_stat.ctime = 0;

            //error_stat = ICdvdCheckLoadError();

            if (error_stat == 1)
            {
                cdvd_stat.error_cnt = 10;

                return;
            }
            else
            {

                if (error_stat == -1)
                {

                    return;
                }
            }

            ICdvdTransFinishedData();

            cdvd_stat.end_size += cdvd_stat.now_size;

            if (cdvd_stat.size <= cdvd_stat.end_size)
            {

                cdvd_stat.stat = CDVD_STAT_FREE;
                cdvd_stat.start_pos = (cdvd_stat.start_pos + 1) % 32;
                cdvd_stat.buf_use_num--;
                cdvd_stat.cmd_on = 0;
            }

            if (cdvd_stat.adpcm_req)
            {

                ICdvdAdpcmLoad();
                cdvd_stat.stat = CDVD_STAT_STREAMING;
            }
            else
            {
                if (cdvd_stat.cmd_on)
                {

                    if (!ICdvdCheckBufStatus())
                    {
                        cdvd_stat.stat = CDVD_STAT_TRANS_WAIT;
                        return;
                    }

                    ICdvdDataReadOnceSector(&rq);
                }
            }
        }//else
        {
            if (cdvd_stat.ctime >= 0x960)
            {
            retry:
                /*if (sceCdBreak()) {

                } else 
                {
                    cdvd_stat.ctime = 0;
                    cdvd_stat.error_cnt = 10;
                    return;
                    goto retry;
                }*/
            }//else {
            //cdvd_stat.ctime++;
            //}
        }
        return;
    }

    if (cdvd_stat.stat == CDVD_STAT_STREAMING)
    {
        int pos = 0xff;

        if (cdvd_stat.adpcm[0].now_load != 0)
        {
            pos = 0;
        }

        else
        {
            if (cdvd_stat.adpcm[1].now_load != 0)
            {
                pos = 1;
            }

            else
            {
                return;
            }
        }

        if (cdvd_stat.adpcm[pos].req_type == 1)
        {
            IAdpcmLoadEndPreOnly(pos);
        }

        else
        {
            SetLoopFlgSize(cdvd_stat.adpcm[pos].size_now, (u_int *) cdvd_stat.adpcm[pos].taddr, 0);
            IAdpcmLoadEndStream(0);
        }
        cdvd_stat.adpcm[pos].now_load = 0;
        cdvd_stat.adpcm[pos].req_type = 0;
        cdvd_stat.adpcm[pos].req_type = 0;

        if (!cdvd_stat.adpcm[pos ^ 1].req_type)
        {
            cdvd_stat.adpcm_req = 0;
        }

        cdvd_stat.stat = 0;
    }
}

static void ICdvdTransCheck()
{
    int i;

    for (i = 0; i < 2; ++i)
    {
        if (cdvd_trans[i].stat == CDVD_STAT_STREAMING)
        {

            switch (cdvd_trans[i].tmem)
            {
                case TRANS_MEM_EE:
                    //if (sceSifDmaStat(cdvd_trans[i].tid) < 0)
                    {
                        if (cdvd_trans[i].ltrans)
                        {
                            cdvd_trans[i].stat = CDVD_STAT_FREE;
                            ICdvdSetRetStat(cdvd_trans[i].id, CDVD_LS_FINISHED);
                        }
                        else
                        {
                            cdvd_trans[i].stat = CDVD_STAT_TRANS_WAIT;
                        }
                    }
                    break;

                case TRANS_MEM_IOP:
                    if (cdvd_trans[i].ltrans)
                    {
                        cdvd_trans[i].stat = CDVD_STAT_FREE;
                        ICdvdSetRetStat(cdvd_trans[i].id, CDVD_LS_FINISHED);
                    }
                    else
                    {
                        cdvd_trans[i].stat = CDVD_STAT_TRANS_WAIT;
                    }
                    break;

                case TRANS_MEM_SPU:
                    //if (sceSdVoiceTransStatus(cdvd_trans[i].tid, 0))
                    {
                        if (cdvd_trans[i].ltrans)
                        {
                            cdvd_trans[i].stat = CDVD_STAT_FREE;
                            ICdvdSetRetStat(cdvd_trans[i].id, CDVD_LS_FINISHED);
                        }
                        else
                        {
                            cdvd_trans[i].stat = CDVD_STAT_TRANS_WAIT;
                        }
                    }
                    break;
            }
        }
    }
}

static void ICdvdTransFinishedData()
{
    //sceSifDmaData sdd;
    int oldstat;
    u_int trans_size;

    switch (cdvd_req[cdvd_stat.start_pos].tmem)
    {
        case TRANS_MEM_EE:
            cdvd_trans[cdvd_stat.now_lbuf].stat = 2;
            cdvd_trans[cdvd_stat.now_lbuf].id = cdvd_req[cdvd_stat.start_pos].id;
            cdvd_trans[cdvd_stat.now_lbuf].tmem = cdvd_req[cdvd_stat.start_pos].tmem;
            break;

        case TRANS_MEM_IOP:
            break;

        case TRANS_MEM_SPU:
            cdvd_trans[cdvd_stat.now_lbuf].tid = 1;

            if (cdvd_stat.now_size % 64)
                trans_size =
                    (cdvd_stat.now_size + 64) - (cdvd_stat.now_size % 64);
            else
                trans_size = cdvd_stat.now_size;

            /*if (!sceSdVoiceTransStatus(cdvd_trans[cdvd_stat.now_lbuf].tid, 0))
            sceSdVoiceTransStatus(cdvd_trans[cdvd_stat.now_lbuf].tid, 1);

        while (sceSdVoiceTrans(
                   cdvd_trans[cdvd_stat.now_lbuf].tid,
                   0,
                   load_buf_table[cdvd_stat.now_lbuf],
                   ((u_int)cdvd_req[cdvd_stat.start_pos].taddr + cdvd_stat.end_size),
                   trans_size)
            < 0);*/
            //sceSdSetTransIntrHandler(cdvd_trans[cdvd_stat.now_lbuf].tid, ICdvdSeTransCB, 0);
            cdvd_trans[cdvd_stat.now_lbuf].stat = 2;
            cdvd_trans[cdvd_stat.now_lbuf].id =
                cdvd_req[cdvd_stat.start_pos].id;
            cdvd_stat.vtrans_flg = 0;
            cdvd_trans[cdvd_stat.now_lbuf].tmem =
                cdvd_req[cdvd_stat.start_pos].tmem;
            break;
    }
}

static int ICdvdSeTransCB(int channel, void* data)
{
    cdvd_stat.vtrans_flg = 0;
    //sceSdSetTransIntrHandler(1, 0, 0);
    return 1;
}


void ICdvdBreak()
{    
    cdvd_stat.stat = 0;
    cdvd_stat.adpcm_req = 0;
}

static int ICdvdSectorLoad(CDVD_REQ_BUF *req_bufp)
{
    if (!ICdvdCheckBufStatus())
        return 0;

    //cdvd_stat.start = req_bufp->start_sector + cdvd_stat.cdlf.lsn;
    cdvd_stat.size = req_bufp->size_sector;
    cdvd_stat.file_no = req_bufp->file_no;
    cdvd_stat.end_size = 0;
    cdvd_stat.id = req_bufp->id;

    ICdvdDataReadOnceSector(req_bufp);
    cdvd_stat.cmd_on = 1;
    ICdvdSetRetStat(cdvd_stat.id, CDVD_LS_LOADING);
    return 1;
}

static int ICdvdCheckBufStatus()
{
    int i;

    for (i = 0; i < 2; i++)
    {
        if (cdvd_trans[cdvd_stat.now_lbuf].stat == 0)
        {
            return 1;
        }
        else if (cdvd_trans[cdvd_stat.now_lbuf].stat == 4)
        {
            continue;
        }
        else if (cdvd_trans[cdvd_stat.now_lbuf].stat == 3)
        {
            return 1;
        }

        cdvd_stat.now_lbuf = (cdvd_stat.now_lbuf + 1) % 2;
    }

    return 0;
}

static void ICdvdAddCmd(IOP_COMMAND *icp)
{
    CDVD_REQ_BUF req_buf = {0};

    req_buf.req_type = icp->cmd_no;
    req_buf.file_no = icp->data1;
    req_buf.start_sector = icp->data2;
    req_buf.size_sector = icp->data3;

    if (icp->data5 >= TRANS_MEM_NUM)
        req_buf.tmem = TRANS_MEM_EE;
    else
        req_buf.tmem = icp->data5;

    if (req_buf.tmem == TRANS_MEM_SPU)
    {
        req_buf.taddr = (u_int*)SeGetSndBufTop(icp->data4);
        req_buf.se_buf_no = icp->data4;
    }
    else
    {
        req_buf.taddr = (u_int *) icp->data4;
    }

    req_buf.id = icp->data6;
    if (cdvd_stat.buf_use_num < 32)
    {
        SDL_LockMutex(cdvd_stat.lock);
        cdvd_req[cdvd_stat.req_pos] = req_buf;
        cdvd_stat.req_pos = (cdvd_stat.req_pos + 1) % 32;
        cdvd_stat.buf_use_num++;
        ICdvdSetRetStat(req_buf.id, CDVD_STAT_LOADING);
        SDL_UnlockMutex(cdvd_stat.lock);
    }
}

static void ICdvdTransSe(IOP_COMMAND *icp)
{
    u_int size;
    u_int addr;

    size = icp->data1;
    addr = snd_buf_top[icp->data2];
    //while (sceSdVoiceTrans(1, 0, (u_char*)load_buf_table[0], (u_int*)addr, size) < 0)
    //  ;
    iop_stat.cdvd.se_trans = 1;
    SeSetStartPoint(icp->data2, icp->data3);
}

int ICdvdTransSeEnd()
{
    /*if (iop_stat.cdvd.se_trans == 1) {
    if (sceSdVoiceTransStatus(1, 0) == 1) {
            iop_stat.cdvd.se_trans = 2;
        }
    } else if (iop_stat.cdvd.se_trans == 2) {
    }*/
}

static void ICdvdTransSeInit()
{
    iop_stat.cdvd.se_trans = 0;
}

static void ICdvdExecCmd()
{
    if (cdvd_stat.buf_use_num)
    {
        if (ICdvdExecCmdSub(&cdvd_req[cdvd_stat.start_pos]))
        {
            cdvd_stat.error_cnt = 0;
        }
        else
        {
        }
    }
    else
    {
    }
}

static int ICdvdExecCmdSub(CDVD_REQ_BUF *req_bufp)
{
    int ret_param = 0;

    switch (req_bufp->req_type)
    {
        case IC_CDVD_LOAD:
            cdvd_stat.stat = CDVD_STAT_LOADING;
            break;
        case IC_CDVD_LOAD_SECT:
            if (ICdvdSectorLoad(req_bufp))
            {
                cdvd_stat.stat = CDVD_STAT_LOADING;
                ret_param = 1;
            }
            break;
        case IC_CDVD_SEEK:
            cdvd_stat.stat = CDVD_STAT_SEEKING;
            break;
    }

    return ret_param;
}

static void ICdvdSetRetStat(int id, u_char stat)
{

    if (id >= 0 && id < 32)
    {
        iop_stat.cdvd.fstat[id].stat = stat;
    }
    else
    {
    }
}

static void ICdvdSetRetStatFinish(int id)
{
    if (id >= 0 && id < 32)
    {
        iop_stat.cdvd.fstat[id].stat = 0;
        cdvd_stat.id = (cdvd_stat.id + 1) % 32;
    }
    else
    {
    }
}

void ICdvdSetRetStatClear()
{
    CDVD_LOAD_STAT *clsp;
    int i;

    for (i = 0, clsp = iop_stat.cdvd.fstat; i < 32; i++, clsp++)
    {
        clsp->stat = CDVD_LS_FINISHED;
    }
}

void ICdvdLoadReqPcm(u_int lsn, u_int size_sec, void *buf, u_char pre)
{
    cdvd_stat.pcm.start = lsn;
    cdvd_stat.pcm.size_now = size_sec;
    cdvd_stat.pcm.taddr = (u_int *) buf;
    if (pre)
    {
        cdvd_stat.pcm_pre = 1;
        cdvd_stat.pcm_pre_end = 0;
    }
    else
    {
        cdvd_stat.pcm_pre = 0;
    }
    cdvd_stat.pcm_req = 1;
}

void ICdvdLoadReqAdpcm(int lsn, u_int size_now, void *buf, u_char channel, int req_type, int endld_flg)
{
    cdvd_stat.adpcm[channel].start = lsn;
    cdvd_stat.adpcm[channel].size_now = size_now;
    cdvd_stat.adpcm[channel].read_now = 0;
    cdvd_stat.adpcm[channel].taddr = (u_int *) buf;
    cdvd_stat.adpcm[channel].req_type = req_type;
    cdvd_stat.adpcm[channel].endld_flg = endld_flg;
    cdvd_stat.adpcm_req = 1;
}