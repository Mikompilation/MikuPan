#include "iopcdvd.h"

#include "common/memory_addresses.h"
#include "iop/iopsys.h"
#include "os/eeiop/eeiop.h"

#include <stdlib.h>

CDVD_STAT cdvd_stat;
CDVD_REQ_BUF cdvd_req[32];
CDVD_LOAD_STAT load_stat[32];
CDVD_TRANS_STAT cdvd_trans[2];
u_int *load_buf_table[2];

void ICdvdInit(int reset);
void ICdvdBreak();
static void ICdvdInitOnce();
static void ICdvdInitSoftReset();
static void ICdvdAddCmd(IOP_COMMAND *icp);
static void ICdvdTransSe(IOP_COMMAND *icp);
static void ICdvdTransSeInit();
static void ICdvdSetRetStat(int id, u_char stat);

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

unsigned char *LoadImgHdFile();
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

    ImgHdAddress = LoadImgHdFile();
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
            ReadFileInArchive(rq->start_sector, rq->size_byte, rq->taddr);
            break;
        case TRANS_MEM_IOP:// IOP
            info_log("CDVD transfer to IOP unimplemented");
            break;
        case TRANS_MEM_SPU:// SPU
            info_log("CDVD transfer to SPU unimplemented");
            break;
    }

    SDL_LockMutex(cdvd_stat.lock);
    cdvd_stat.start_pos = (cdvd_stat.start_pos + 1) % 32;
    cdvd_stat.buf_use_num--;
    ICdvdSetRetStat(rq->id, CDVD_LS_FINISHED);
    SDL_UnlockMutex(cdvd_stat.lock);
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
}
// ------

void ICdvdBreak()
{
}

static void ICdvdAddCmd(IOP_COMMAND *icp)
{
    CDVD_REQ_BUF req_buf;

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
