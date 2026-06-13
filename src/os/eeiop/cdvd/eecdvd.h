#ifndef OS_EEIOP_CDVD_EECDVD_H
#define OS_EEIOP_CDVD_EECDVD_H

#include <stdint.h>

#include "typedefs.h"

typedef struct {
	u_int start;
	u_int size;
} IMG_ARRANGEMENT;

void CdvdInit();
void CdvdInitSoftReset();
int LoadReq(int file_no, uint64_t ps2_addr);
int LoadReqToHostPointer(int file_no, void *host_addr);
int64_t LoadReqGetHostPointerEnd(int file_no, void *host_addr, int *id);
int LoadReqSe(int file_no, u_char se_type);
int64_t LoadReqNSector(int file_no, int sector, int size, int64_t ps2_addr);
int64_t LoadReqNSectorToHostPointer(int file_no, int sector, int size, void *host_addr);
int LoadReqNFno(int file_no, int64_t addr);
int LoadReqBFno(int file_no, int64_t addr);
u_int LoadReqBFnoGetAddr(int file_no, int64_t addr);
int IsLoadEndAll();
int IsLoadEnd(int id);
void LoadEndFlgRenew();
u_char IsExistLoadReq();
IMG_ARRANGEMENT* GetImgArrangementP(int file_no);

#ifdef BUILD_EU_VERSION
int LoadReqLanguage(int file_no, u_int addr);
#endif

#endif // OS_EEIOP_CDVD_EECDVD_H
