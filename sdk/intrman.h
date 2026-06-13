#ifndef MIKUPAN_IOP_INTRMAN_H
#define MIKUPAN_IOP_INTRMAN_H

#define INUM_DMA_4 36
#define INUM_DMA_7 40
#define INUM_SPU   9

int CpuEnableIntr(void);
int CpuSuspendIntr(int* oldstat);
int CpuResumeIntr(int oldstat);
int EnableIntr(int intr);

#endif
