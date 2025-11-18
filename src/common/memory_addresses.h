#ifndef MIKUPAN_MEMORY_ADDRESSES_H
#define MIKUPAN_MEMORY_ADDRESSES_H

//#define FontTextAddress     0x01e30000
extern void* FontTextAddress; /// PK2 font texture pack

// #define ImgHdAddress        0x012f0000
extern void* ImgHdAddress; /// IMG_HD.BIN

// #define IG_MSG_OBJ_ADDRESS  0x0084a000
extern void* IG_MSG_OBJ_ADDRESS; ///

// #define EFFECT_ADDRESS      0x01e90000
extern void* EFFECT_ADDRESS; ///

// #define PBUF_ADDRESS        0x00720000
//extern void* PBUF_ADDRESS; /// Packet buffer

// #define SPRITE_ADDRESS 0xa30000
extern void* SPRITE_ADDRESS;

/// 0x1d05140
extern void* PL_BGBG_PK2_ADDRESS;

/// 0x1ce0000
extern void* PL_STTS_PK2_ADDRESS;

/// 0x1d59630
extern void* PL_PSVP_PK2_ADDRESS;

/// 0x1d15600
extern void* PL_SAVE_PK2_ADDRESS;

/// 0x1d28c80
extern void* SV_PHT_PK2_ADDRESS;

/// 0xc80000
extern void* M_SLCT_CMN_PK2_ADDRESS;

/// 0xcc0470
extern void* M_SLCT_STY_PK2_ADDRESS;

/// 0xddc430
extern void* PL_OPTI_PK2_ADDRESS;

/// 0xd4a850
extern void* M_SLCT_BTL_CHR_PK2_ADDRESS;

/// 0xdcb100
extern void* M_SLCT_BTL_MSN_PK2_ADDRESS;

/// 0x1e05b00
extern void* PL_SMAP_PK2_ADDRESS;

/// 0x1e2f700
extern void* PL_PLAY_PK2_ADDRESS;

/// 0x1d266c0
extern void* PL_PLDT_PK2_ADDRESS;

/// 0x1d15600
extern void* PL_MTOP_PK2_ADDRESS;

/// 0x1f1c000
extern void* N_LOAD_PK2_ADDRESS;

/// 0x1fc8000
extern void* HAND_PK2_ADDRESS;

/// 0x1d88100
extern void* PL_FNDR_PK2_ADDRESS;

/// 0x1df2100
extern void* PL_LIFE_PK2_ADDRESS;

/// 0x1fa8000
extern void* ENEDMG_PK2_ADDRESS;

/// 0x1e85000
extern void* PHOTO001_PK2_ADDRESS;

/// 0x04300000
extern void* TEST2D_PK2_ADDRESS;

/// 0x04610000
extern void* TEST_ROOM_CHECK_ADDRESS;

/// 0x14b0000
extern void* MPEG_WORK_ADDRESS;

/// 0x1e90000
extern void* MISSION_TITLE_CARD_ADDRESS;

#define VNBufferAddress     0x00420000
#define CachedBuffer        0x20000000
#define UnCachedBuffer      0x30000000
#define VU0_ADDRESS         0x11000000

#define MC_WORK_ADDRESS 0x420000

#define SPRITE_ADDR_1 0x0c80000
#define SPRITE_ADDR_2 PL_SAVE_PK2_ADDRESS
#define SPRITE_ADDR_3 0x1d28c80
#define SPRITE_ADDR_4 0x1d59630



#endif //MIKUPAN_MEMORY_ADDRESSES_H