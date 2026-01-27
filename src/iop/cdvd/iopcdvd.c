#include "iopcdvd.h"

#include "../../mikupan/mikupan_logging_c.h"
#include "SDL3/SDL_mutex.h"
#include "enums.h"
#include "iop/iopmain.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_memory.h"
#include "os/eeiop/eeiop.h"
#include "iop/adpcm/iopadpcm.h"

#include <stdio.h>
#include <stdlib.h>

CDVD_STAT cdvd_stat;
CDVD_REQ_BUF cdvd_req[32];
CDVD_LOAD_STAT load_stat[32];
CDVD_TRANS_STAT cdvd_trans[2];
u_int *load_buf_table[2];

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
    load_buf_table[0] = malloc(0x64000);

    if (load_buf_table[0])
    {
        load_buf_table[1] = load_buf_table[0] + 0x32000;
        iop_stat.cdvd.ld_addr = load_buf_table[0];
    }

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

// --- these functions are more heavily modified for PC ---
void ICdvdDoTransfer(CDVD_REQ_BUF *rq)
{
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

    SDL_LockMutex(cdvd_stat.lock);
    cdvd_stat.start_pos = (cdvd_stat.start_pos + 1) % 32;
    cdvd_stat.buf_use_num--;
    ICdvdSetRetStat(rq->id, CDVD_LS_FINISHED);
    SDL_UnlockMutex(cdvd_stat.lock);
}

static void ICdvdAdpcmLoad()
{
    u_char pos;

    pos = 0xff;
    if (cdvd_stat.adpcm[0].req_type >= cdvd_stat.adpcm[1].req_type) {
        pos = 0;
    } else {
        pos = 1;
    }

    MikuPan_ReadFileInArchive64(cdvd_stat.adpcm[pos].start, cdvd_stat.adpcm[pos].size_now, &AdpcmIopBuf[pos]);

    cdvd_stat.adpcm[0].now_load = 1;
    cdvd_stat.adpcm_req = 1;
}

void ICdvdMain()
{
    CDVD_REQ_BUF rq;
    bool found_work = false;

    SDL_LockMutex(cdvd_stat.lock);

    if (cdvd_stat.buf_use_num)
    {
        rq = cdvd_req[cdvd_stat.start_pos];
        found_work = true;
    }

    SDL_UnlockMutex(cdvd_stat.lock);

    if (found_work)
    {
        ICdvdDoTransfer(&rq);
    }
                
    int pos = 0xff;
            
    if (cdvd_stat.adpcm_req)
    {
        ICdvdAdpcmLoad();    
        cdvd_stat.stat = 2;    
    } 
        
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
        IAdpcmLoadEndPreOnly(0);        
    }

    else 
    {
        SetLoopFlgSize(cdvd_stat.adpcm[pos].size_now, (u_int*)cdvd_stat.adpcm[pos].taddr, 0);        
        IAdpcmLoadEndStream(0);
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
// ------

void ICdvdBreak()
{
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
        //req_buf.taddr = (u_int*)SeGetSndBufTop(icp->data4);
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
}

static void ICdvdTransSeInit()
{
}

static void ICdvdSetRetStat(int id, u_char stat)
{
    if (id < 32)
        iop_stat.cdvd.fstat[id].stat = stat;
}

void ICdvdLoadReqPcm(u_int lsn, u_int size_sec, void* buf, u_char pre)
{
    cdvd_stat.pcm.start = lsn;
    cdvd_stat.pcm.size_now = size_sec;
    cdvd_stat.pcm.taddr = buf;
    if (pre) {
        cdvd_stat.pcm_pre = 1;
        cdvd_stat.pcm_pre_end = 0;
    } else {
        cdvd_stat.pcm_pre = 0;
    }
    cdvd_stat.pcm_req = 1;
}

void ICdvdLoadReqAdpcm(int lsn, u_int size_now, void* buf, u_char channel, int req_type, int endld_flg)
{
    cdvd_stat.adpcm[channel].start = lsn;
    cdvd_stat.adpcm[channel].size_now = size_now;
    cdvd_stat.adpcm[channel].read_now = 0;
    cdvd_stat.adpcm[channel].taddr = &AdpcmIopBuf[channel];
    cdvd_stat.adpcm[channel].req_type = req_type;
    cdvd_stat.adpcm[channel].endld_flg = endld_flg;
    cdvd_stat.adpcm_req = 1;
}
