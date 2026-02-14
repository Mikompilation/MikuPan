#ifndef MIKUPAN_MEMORY_ADDRESSES_H
#define MIKUPAN_MEMORY_ADDRESSES_H

#include <stdint.h>

extern unsigned char ps2_virtual_ram[];
extern unsigned char ps2_virtual_scratchpad[];

int64_t MikuPan_GetHostAddress(int offset);
void* MikuPan_GetHostPointer(int offset);
void MikuPan_InitPs2Memory();
int MikuPan_IsPs2MemoryPointer(int64_t address);
int MikuPan_SanitizePs2Address(int address);
int MikuPan_IsPs2AddressMainMemoryRange(int address);
int MikuPan_GetPs2OffsetFromHostPointer(void* ptr);

#define FontTextAddress                 0x01e30000
#define ImgHdAddress                    0x012f0000
#define IG_MSG_OBJ_ADDRESS              0x0084a000
#define EFFECT_ADDRESS                  0x01e90000
#define PBUF_ADDRESS                    0x00720000
#define SPRITE_ADDRESS                  0x00a30000
#define PL_BGBG_PK2_ADDRESS             0x01d05140
#define PL_STTS_PK2_ADDRESS             0x01ce0000
#define PL_PSVP_PK2_ADDRESS             0x01d59630
#define PL_SAVE_PK2_ADDRESS             0x01d15600
#define SV_PHT_PK2_ADDRESS              0x01d28c80
#define M_SLCT_CMN_PK2_ADDRESS          0x00c80000
#define M_SLCT_STY_PK2_ADDRESS          0x00cc0470
#define PL_OPTI_PK2_ADDRESS             0x00ddc430
#define M_SLCT_BTL_CHR_PK2_ADDRESS      0x00d4a850
#define M_SLCT_BTL_MSN_PK2_ADDRESS      0x00dcb100
#define PL_SMAP_PK2_ADDRESS             0x01e05b00
#define PL_PLAY_PK2_ADDRESS             0x01e2f700
#define PL_PLDT_PK2_ADDRESS             0x01d266c0
#define PL_MTOP_PK2_ADDRESS             0x01d15600
#define N_LOAD_PK2_ADDRESS              0x01f1c000
#define HAND_PK2_ADDRESS                0x01fc8000
#define PL_FNDR_PK2_ADDRESS             0x01d88100
#define PL_LIFE_PK2_ADDRESS             0x01df2100
#define ENEDMG_PK2_ADDRESS              0x01fa8000
#define PHOTO001_PK2_ADDRESS            0x01e85000
#define MPEG_WORK_ADDRESS               0x014b0000
#define MISSION_TITLE_CARD_ADDRESS      0x01e90000
#define PLYR_FILE_ADDRESS               0x009a0000
#define PL_ALBM_FSM_PK2_ADDRESS         0x00c80000
#define PL_ALBM_SIDE_1_ADDRESS          0x01dc8570
#define PL_PHOT_PK2_ADDRESS             0x01d573b0
#define PLAYER_ANM_ADDRESS              0x00870000
#define ENE_DMG_TEX_ADDRESS             0x00ac8000
#define SCENE_LOAD_ADDRESS              0x01090000
#define MSN_TITLE_DAT_ADDRESS           0x007F0000
#define MSN_TITLE_DAT_ADDRESS_1         0x007E0000
#define MSN_TITLE_DAT_ADDRESS_2         0x007F8000
#define MSN_TITLE_DAT_ADDRESS_3         0x00E00000
#define MSN_TITLE_DAT_ADDRESS_4         0x00E80000
#define MSN_TITLE_DAT_ADDRESS_5         0x00C40000
#define MSN_TITLE_DAT_ADDRESS_6         0x00C4C000
#define MSN_TITLE_DAT_ADDRESS_7         0x00C58000
#define MSN_TITLE_DAT_ADDRESS_8         0x01000000
#define MSN_TITLE_DAT_ADDRESS_9         0x01020000
#define VNBufferAddress                 0x00420000
#define MC_WORK_ADDRESS                 0x00420000
#define OBJ_WRK_ADDRESS                 0x0042fa00
#define MIM_BUF_BASE_ADDRESS            0x01310000
#define ENEMY_ANM_ADDR                  0x01330000
#define ENEMY_DMG_TEX_ADDR              0x013c8000
#define ENEMY_MDL_ADDR                  0x013e0000
#define FLOAT_GHOST_SE_LOAD_ADDR        0x01460000
#define MODEL_ADDRESS                   0x00d80000
#define ANIM_ADDRESS                    0x00b90000
#define ENE_DMG_ADDRESS                 0x00c28000
#define RARE_ENE_SPR_ADDRESS            0x01c90000
#define MAP_DATA_ADDRESS                0x007F8000
#define ENE_DMG_TEX_BASE_ADDRESS        0x98000000
#define FLY_MDL_ADDRESS                 0xd8000000
#define MSN00TTL_PK2_ADDRESS            0x1e900000
#define STORY_WORK_SAVE_ADDRESS         0x10900000
#define PBUF_ADDRESS                    0x00720000

/* DEVKIT EXTANDED RAM */
#define TEST2D_PK2_ADDRESS              0x007F0000 /* 0x04300000 */
#define TEST_ROOM_CHECK_ADDRESS         0x007F0000 /* 0x04610000 */
/* DEVKIT EXTANDED RAM */

#define CachedBuffer                    0x20000000
#define UnCachedBuffer                  0x30000000
#define VU0_ADDRESS                     0x11000000

#define SCRATCHPAD_ADDRESS              0x70000000

#endif //MIKUPAN_MEMORY_ADDRESSES_H