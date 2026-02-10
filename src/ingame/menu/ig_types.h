#ifndef INGAME_MENU_IG_TYPES_H
#define INGAME_MENU_IG_TYPES_H

#include "typedefs.h"
#include "os/system.h"

typedef struct {
	u_char adr_no;
	u_char msn_no;
	u_char room;
	u_char status;
	u_short subject[3][2];
	u_int score;
	sceCdCLOCK time;
} PICTURE_WRK;

typedef struct {
	PICTURE_WRK pic_inf[10];
	u_short pic_num;
	u_short best_score;
} SAVE_RANK;

typedef struct { // 0x5
  /* 0x0 */ u_char mode;
  /* 0x1 */ u_char csr_bak;
  /* 0x2 */ u_char star;
  /* 0x3 */ u_char cnt;
  /* 0x4 */ char exist_s_rank;
} MENU_RANK;

#endif // INGAME_MENU_IG_TYPES_H
