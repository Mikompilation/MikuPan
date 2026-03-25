#ifndef GRAPHICS_MOV_MOVIE_H
#define GRAPHICS_MOV_MOVIE_H

#include "typedefs.h"

#include "sce/mpeg/libmpeg.h"
#include "sce/mpeg/vibuf.h"
#include "sce/mpeg/vobuf.h"
#include "sce/mpeg/readbuf.h"
#include "sce/mpeg/videodec.h"
#include "sce/mpeg/audiodec.h"
#include "sce/libcdvd.h"

enum SCENE_NO {
	SCENE_NO_0_01_0 = 0,
	SCENE_NO_0_02_0 = 1,
	SCENE_NO_0_02_2 = 2,
	SCENE_NO_0_03_0 = 3,
	SCENE_NO_0_03_1 = 4,
	CHAPTER0_SCENE = 5,
	SCENE_NO_1_00_0 = 5,
	SCENE_NO_1_01_0 = 6,
	SCENE_NO_1_03_0 = 7,
	SCENE_NO_1_03_1 = 8,
	SCENE_NO_1_03_4 = 9,
	SCENE_NO_1_07_0 = 10,
	SCENE_NO_1_08_0 = 11,
	SCENE_NO_1_09_0 = 12,
	SCENE_NO_1_10_0 = 13,
	SCENE_NO_1_10_1 = 14,
	SCENE_NO_1_10_2 = 15,
	SCENE_NO_1_12_0 = 16,
	SCENE_NO_1_15_0 = 17,
	SCENE_NO_1_16_0 = 18,
	SCENE_NO_1_17_0 = 19,
	SCENE_NO_1_18_0 = 20,
	SCENE_NO_1_22_0 = 21,
	SCENE_NO_1_23_0 = 22,
	SCENE_NO_1_24_0 = 23,
	SCENE_NO_1_25_0 = 24,
	SCENE_NO_1_26_0 = 25,
	SCENE_NO_1_28_0 = 26,
	SCENE_NO_1_30_0 = 27,
	SCENE_NO_1_30_1 = 28,
	SCENE_NO_1_32_0 = 29,
	SCENE_NO_1_33_0 = 30,
	SCENE_NO_1_33_1 = 31,
	SCENE_NO_1_33_2 = 32,
	SCENE_NO_1_34_0 = 33,
	SCENE_NO_1_41_0 = 34,
	CHAPTER1_SCENE = 35,
	SCENE_NO_2_01_0 = 35,
	SCENE_NO_2_01_3 = 36,
	SCENE_NO_2_02_0 = 37,
	SCENE_NO_2_03_0 = 38,
	SCENE_NO_2_04_0 = 39,
	SCENE_NO_2_05_0 = 40,
	SCENE_NO_2_06_0 = 41,
	SCENE_NO_2_06_1 = 42,
	SCENE_NO_2_06_2 = 43,
	SCENE_NO_2_07_0 = 44,
	SCENE_NO_2_07_1 = 45,
	SCENE_NO_2_07_2 = 46,
	SCENE_NO_2_09_0 = 47,
	SCENE_NO_2_09_1 = 48,
	SCENE_NO_2_09_2 = 49,
	SCENE_NO_2_10_0 = 50,
	SCENE_NO_2_11_0 = 51,
	SCENE_NO_2_13_0 = 52,
	SCENE_NO_2_13_1 = 53,
	SCENE_NO_2_13_2 = 54,
	SCENE_NO_2_14_0 = 55,
	SCENE_NO_2_14_1 = 56,
	SCENE_NO_2_14_2 = 57,
	SCENE_NO_2_14_3 = 58,
	SCENE_NO_2_15_0 = 59,
	SCENE_NO_2_16_0 = 60,
	SCENE_NO_2_17_0 = 61,
	SCENE_NO_2_17_1 = 62,
	CHAPTER2_SCENE = 63,
	SCENE_NO_3_01_0 = 63,
	SCENE_NO_3_02_0 = 64,
	SCENE_NO_3_03_0 = 65,
	SCENE_NO_3_04_0 = 66,
	SCENE_NO_3_05_0 = 67,
	SCENE_NO_3_06_0 = 68,
	SCENE_NO_3_07_0 = 69,
	SCENE_NO_3_08_0 = 70,
	SCENE_NO_3_08_1 = 71,
	SCENE_NO_3_09_0 = 72,
	SCENE_NO_3_10_0 = 73,
	SCENE_NO_3_11_0 = 74,
	CHAPTER3_SCENE = 75,
	SCENE_NO_4_01_0 = 75,
	SCENE_NO_4_01_1 = 76,
	SCENE_NO_4_02_0 = 77,
	SCENE_NO_4_03_0 = 78,
	SCENE_NO_4_03_1 = 79,
	SCENE_NO_4_04_0 = 80,
	SCENE_NO_4_04_1 = 81,
	SCENE_NO_4_04_2 = 82,
	SCENE_NO_4_05_0 = 83,
	SCENE_NO_4_06_0 = 84,
	SCENE_NO_4_07_0 = 85,
	SCENE_NO_4_08_0 = 86,
	SCENE_NO_4_09_0 = 87,
	SCENE_NO_4_10_0 = 88,
	SCENE_NO_4_11_0 = 89,
	SCENE_NO_4_12_0 = 90,
	CHAPTER4_SCENE = 91,
	SCENE_NO_5_01_0 = 91,
	SCENE_NO_5_02_0 = 92,
	CHAPTER5_SCENE = 93,
	SCENE_NO_4_90_0 = 93,
	SCENE_NO_4_90_1 = 94,
	SCENE_NO_9_00_0 = 95,
	SCENE_NO_9_00_1 = 96,
	SPECIAL_SCENE = 97,
	SCENE_NO_9_10_0 = 98,
	SCENE_NO_9_20_0 = 99,
	SCENE_NO_MAX = 99
};

typedef struct {
	int play_event_no;
	int play_event_sta;
} MOVIE_WRK;

typedef union {
    u_long128 q;
    u_long l[2];
    u_int i[4];
    u_short s[8];
    u_char c[16];
} QWORD;

typedef struct {
    int isOnCD;
    int size;
    sceCdlFILE fp;
    u_char *iopBuf;
    int fd;
} StrFile;

// extern ReadBuf *readBuf;
extern u_int scene_bg_color;
// extern int isWithAudio;
// extern char *commandname;
// extern char *VERSION;
// extern char *mpegName[0];
// extern u_char mpeg_vol_rate[0];
// extern int isCountVblank;
// extern int vblankCount;
// extern int isFrameEnd;
// extern int oddeven;
// extern int handler_error;
// extern int isStrFileInit;
// extern VoBuf voBuf;
extern MOVIE_WRK movie_wrk;
extern int thread_id;
extern u_int controller_val;
extern int videoDecTh;
// extern int defaultTh;
// extern StrFile infile;
// extern VideoDec videoDec;
// extern AudioDec audioDec;
extern int frd;
// extern sceGsDBuff db;
extern int play_mov_no;

void MovieInitWrk();
void ReqMpegEvent(int no);
int PlayMpegEvent();
u_int movie(char *name);
void switchThread();
void initMov(char *bsfilename);
void ErrMessage(char *message);
void proceedAudio();
int MoviePlay(int scene_no);
void MovieTest(int scene_no);
int PadSyncCallback2();
void movVblankPad();
void ReqLogoMovie();
int audioDecCreate(AudioDec *ad, u_char *buff, int buffSize, int iopBuffSize);
int audioDecDelete(AudioDec *ad);
void audioDecPause(AudioDec *ad);
void audioDecResume(AudioDec *ad);
void audioDecStart(AudioDec *ad);
void audioDecReset(AudioDec *ad);
void audioDecBeginPut(AudioDec *ad, u_char **ptr0, int *len0, u_char **ptr1, int *len1);
void audioDecEndPut(AudioDec *ad, int size);
int audioDecIsPreset(AudioDec *ad);
int audioDecSendToIOP(AudioDec *ad);
void clearGsMem(int r, int g, int b, int disp_width, int disp_height);
void setImageTag(u_int *tags, void *image, int index, int image_w, int image_h);
int vblankHandlerM(int val);
int handler_endimage(int val);
void startDisplay(int waitEven);
void endDisplay();
int videoCallback(sceMpeg *mp, sceMpegCbDataStr *str, void *data);
int pcmCallback(sceMpeg *mp, sceMpegCbDataStr *str, void *data);
void readBufCreate(ReadBuf *b);
void readBufDelete(ReadBuf *b);
int readBufBeginPut(ReadBuf *b, u_char **ptr);
int readBufEndPut(ReadBuf *b, int size);
int readBufBeginGet(ReadBuf *b, u_char **ptr);
int readBufEndGet(ReadBuf *b, int size);
int strFileOpen(StrFile *file, char *filename);
int strFileClose(StrFile *file);
int strFileRead(StrFile *file, void *buff, int size);
int getFIFOindex(ViBuf *f, void *addr);
void setD3_CHCR(u_int val);
void setD4_CHCR(u_int val);
void scTag2(QWORD *q, void *addr, u_int id, u_int qwc);
int viBufCreate(ViBuf *f, u_long128 *data, u_long128 *tag, int size, TimeStamp *ts, int n_ts);
int viBufReset(ViBuf *f);
void viBufBeginPut(ViBuf *f, u_char **ptr0, int *len0, u_char **ptr1, int *len1);
void viBufEndPut(ViBuf *f, int size);
int viBufAddDMA(ViBuf *f);
int viBufStopDMA(ViBuf *f);
int viBufRestartDMA(ViBuf *f);
int viBufDelete(ViBuf *f);
int viBufIsActive(ViBuf *f);
int viBufCount(ViBuf *f);
void viBufFlush(ViBuf *f);
int viBufModifyPts(ViBuf *f, TimeStamp *new_ts);
int viBufPutTs(ViBuf *f, TimeStamp *ts);
int viBufGetTs(ViBuf *f, TimeStamp *ts);
int videoDecCreate(VideoDec *vd, u_char *mpegWork, int mpegWorkSize, u_long128 *data, u_long128 *tag, int tagSize, TimeStamp *pts, int n_pts);
void videoDecSetDecodeMode(VideoDec *vd, int ni, int np, int nb);
int videoDecSetStream(VideoDec *vd, int strType, int ch, sceMpegCallback cb, void *data);
void videoDecBeginPut(VideoDec *vd, u_char **ptr0, int *len0, u_char **ptr1, int *len1);
void videoDecEndPut(VideoDec *vd, int size);
void videoDecReset(VideoDec *vd);
int videoDecDelete(VideoDec *vd);
void videoDecAbort(VideoDec *vd);
u_int videoDecGetState(VideoDec *vd);
u_int videoDecSetState(VideoDec *vd, u_int state);
int videoDecPutTs(VideoDec *vd, long int pts_val, long int dts_val, u_char *start, int len);
int videoDecInputCount(VideoDec *vd);
int videoDecInputSpaceCount(VideoDec *vd);
int videoDecFlush(VideoDec *vd);
int videoDecIsFlushed(VideoDec *vd);
void videoDecMain(VideoDec *vd);
int decBs0(VideoDec *vd);
int mpegError(sceMpeg *mp, sceMpegCbDataError *cberror, void *anyData);
int mpegNodata(sceMpeg *mp, sceMpegCbData *cbdata, void *anyData);
int mpegStopDMA(sceMpeg *mp, sceMpegCbData *cbdata, void *anyData);
int mpegRestartDMA(sceMpeg *mp, sceMpegCbData *cbdata, void *anyData);
int mpegTS(sceMpeg *mp, sceMpegCbDataTimeStamp *cbts, void *anyData);
void voBufCreate(VoBuf *f, VoData *data, VoTag *tag, int size);
void voBufDelete(VoBuf *f);
void voBufReset(VoBuf *f);
int voBufIsFull(VoBuf *f);
void voBufIncCount(VoBuf *f);
VoData* voBufGetData(VoBuf *f);
int voBufIsEmpty(VoBuf *f);
VoTag* voBufGetTag(VoBuf *f);
void voBufDecCount(VoBuf *f);
void playMovLoop();

#endif // GRAPHICS_MOV_MOVIE_H
