#include "common.h"
#include "ev_load.h"
#include "main/glob.h"
#include "enums.h"
#include "ingame/map/map_area.h"
#include "ingame/entry/entry.h"
#include "common/memory_addresses.h"
#include "ingame/map/furn_ctl.h"
#include "ingame/map/furn_spe/fspe_acs.h"

MSN_TITLE_WRK mttl_wrk;
EVENT_LOAD_WRK ev_load_wrk;
u_char msn_start_room[];
MSN_LOAD_DAT load_dat_wrk[40];
u_char msn_title_sp_flr_no[];
u_char msn_title_flr_sp_num[];
u_char msn_title_sp_ttl_no[];
u_char msn_title_ttl_sp_num[];
SPRT_SDAT *msn_title_sp_flr[];
SPRT_SDAT *msn_title_sp_ttl[];
SPRT_SDAT msn_title_sp_bak[] = {
    {
        .u = 0,
        .v = 0,
        .w = 512,
        .h = 256,
        .x = 0,
        .y = 0,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 0,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 128,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 128,
        .x = 0,
        .y = 256,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 128,
        .x = 256,
        .y = 256,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 256,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 0,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 128,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 256,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 384,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 512,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
};
MSN_LOAD_DAT *msn_title_load_dat[];

static u_char msn_start_floor[9];

void MissionTitleInit(int msn_no)
{
    DataLoadWrkInit();
    memset(&mttl_wrk, 0, sizeof(mttl_wrk));
    mttl_wrk.mode = 0;
    mttl_wrk.load_mode = 0;
    ingame_wrk.stts |= 0x28;
  
    if (ingame_wrk.game == 0)
    {
        map_wrk.floor = msn_start_floor[msn_no];
        mttl_wrk.load_id = LoadReq(msn_no + 0x26, &MISSION_TITLE_CARD_ADDRESS);
    }
    else if (ingame_wrk.game == 1)
    {
        mttl_wrk.load_id = StageTitleInit();
    }
    MissionStartMapItemInit(msn_no);
    AdpcmMapNoUse();
    return;
}

int MissionTitleMain(int msn_no)
{

    printf("MISSION TITLE MAIN \n");

    switch(mttl_wrk.mode) 
    {
        case MSN_TITLE_MODE_READY:
            if (IsLoadEnd(mttl_wrk.load_id) != 0) 
            {
                mttl_wrk.mode = MSN_TITLE_MODE_IN;
                mttl_wrk.time = 30;
                EAdpcmCmdPlay(0, 0, AO002_SHOU_TITLE_STR, 0, 0x3fff, 0x280, 0xfff, 0);
            }
        break;
        case MSN_TITLE_MODE_IN:
            if (mttl_wrk.time == 0)
            {
                mttl_wrk.mode = MSN_TITLE_MODE_LOAD;
                mttl_wrk.time = 180;
            }
            else
            {
                mttl_wrk.time -= 1;
            }
        break;
        case MSN_TITLE_MODE_LOAD:
            if (mttl_wrk.time != 0) 
            {
                mttl_wrk.time -= 1;
            }

            if (ingame_wrk.game == 0)
            {
                if (MissionTitleLoad(msn_no) != 0 && mttl_wrk.time == 0)
                {
                    mttl_wrk.mode = MSN_TITLE_MODE_OUT;
                    mttl_wrk.time = 30;
                }
            }
            else if (ingame_wrk.game == 1) 
            {
                if (StageTitleLoad() != 0 && mttl_wrk.time == 0)
                {
                    mttl_wrk.mode = MSN_TITLE_MODE_OUT;
                    mttl_wrk.time = 30;
                }
            }
        break;
        case MSN_TITLE_MODE_OUT:
            if (mttl_wrk.time != 0)
            {
                mttl_wrk.time -= 1;
            }
            else
            {
                mttl_wrk.mode = MSN_TITLE_MODE_END_PRE;
                EAdpcmFadeOut(30);
            }
        break;        
        case MSN_TITLE_MODE_END_PRE:
            if (IsEndAdpcmFadeOut() != 0) 
            {
                mttl_wrk.mode = MSN_TITLE_MODE_END;
            }
        break;
  
        case MSN_TITLE_MODE_END:
            ingame_wrk.stts &= 0x80 | 0x40 | 0x10 | 0x4 | 0x2 | 0x1;
            return 1;
        break;
    }
  
    if (mttl_wrk.mode != MSN_TITLE_MODE_READY && mttl_wrk.mode != MSN_TITLE_MODE_END) 
    {
        if (ingame_wrk.game == 0) 
        {
            MissionTitleDisp(msn_no);
        }
        else if (ingame_wrk.game == 1) 
        {
            StageTitleDisp(msn_no);
        }
    }

    return 0;
}

int MissionTitleLoad(int msn_no)
{

    printf("MISSION TITLE LOAD\n");

    if (mttl_wrk.load_mode == 9)
    {
        return 1;
    }
    
    if (mttl_wrk.load_mode == 0)
    {
        mttl_wrk.load_mode = 1;
        
        ReqMsnInitPlyr(msn_no);
        return 0;
    }
    else if (mttl_wrk.load_mode == 1)
    {
        if (MsnInitPlyr())
        {
            mttl_wrk.load_mode = 2;
        }
        return 0;
    }
    
    if (msn_title_load_dat[msn_no][mttl_wrk.load_count].file_no == 0xFFFF)
    {
        mttl_wrk.load_mode = 4;
        mttl_wrk.load_count = 0;
    }
    else if (mttl_wrk.load_mode == 3)
    {
        if (IsLoadEnd(mttl_wrk.load_id))
        {
            MissionDataLoadAfterInit(msn_title_load_dat[msn_no] + mttl_wrk.load_count);
            SetDataLoadWrk(msn_title_load_dat[msn_no] + mttl_wrk.load_count);
            mttl_wrk.load_mode = 2;
            mttl_wrk.load_count++;
        }
    }
    else if (mttl_wrk.load_mode == 2)
    {
        mttl_wrk.load_id = MissionDataLoadReq(&msn_title_load_dat[msn_no][mttl_wrk.load_count]);
        mttl_wrk.load_mode = 3;
    }
    else if (mttl_wrk.load_mode == 4)
    {
        if (mttl_wrk.load_count != 0)
        {
            FloatGhostLoadReq();
            mttl_wrk.load_mode = 6;
            return 0;
        }
        
        RoomMdlLoadReq(NULL, mttl_wrk.load_count, msn_no, msn_start_room[msn_no], 1);
        area_wrk.room[mttl_wrk.load_count] = msn_start_room[msn_no];
        mttl_wrk.load_mode = 5;
    }
    else if (mttl_wrk.load_mode == 5)
    {
        if (RoomMdlLoadWait() == 0)
        {
            return 0;
        }
        
        mttl_wrk.load_mode = 4;
        mttl_wrk.load_count++;
    } 
    else if (mttl_wrk.load_mode == 6)
    {
        if (FloatGhostLoadMain() == 0)
        {
            return 0;
        }
        
        FloatGhostAppearTypeSet(ap_wrk.fgst_no, 0, msn_start_room[msn_no]);
        if (GuardGhostAppearSet() == 0)
        {
            mttl_wrk.load_mode = 9;
        }
        else
        {
            mttl_wrk.load_mode = 8;
        }
    }
    else if (mttl_wrk.load_mode == 7)
    {
        if (DeadGhostLoad() == 0)
        {
            return 0;
        }
        
        if (GuardGhostAppearSet() == 0)
        {
            mttl_wrk.load_mode = 9;
        }
        else
        {
            mttl_wrk.load_mode = 8;
        }
    }
    else
    {
        if (mttl_wrk.load_mode != 8)
        {
            return 0;
        }
        
        if (GuardGhostLoad() != 0)
        {
            mttl_wrk.load_mode = 9;
        }
    }
    
    return 0;
}

int MissionDataLoadReq(MSN_LOAD_DAT* dat)
{
    	
    /* v0 2 */ int ret;
    
    printf("MISSION DATA LOAD REQ\n");

    if (dat->file_type == 2)
    {
        if (dat->addr == 16)
        {
            ap_wrk.fg_se_empty[0] = 1;
        }
        else if (dat->addr == 17)
        {
            ap_wrk.fg_se_empty[1] = 1;
        }
        else if (dat->addr == 18)
        {
            ap_wrk.fg_se_empty[2] = 1;
        }

        if (dat->tmp_no == 0)
        {
            ret = SeFileLoadAndSet(dat->file_no, dat->addr);
        }
        else
        {
            ret = SeFileLoadAndSetFGhost(dat->file_no, dat->addr);
        }
    }
    else
    {
        if (dat->file_type == 9)
        {
            LoadEneDmgTex(dat->tmp_no, (u_int *)(dat->addr + 0x98000));

            ret = LoadReq(dat->file_no, dat->addr);
        }
        else
        {
            ret = LoadReq(dat->file_no, dat->addr);
        }
    }

    return ret;
}

void MissionDataLoadAfterInit(MSN_LOAD_DAT* dat)
{

    printf("MISSION DATA LOAD AFTER INIT\n");

    switch (dat->file_type)
    {
        case 3:
            memcpy(&map_cam_dat, (void *)dat->addr, sizeof(map_cam_dat));
            break;

        
        case 4:
            memcpy(&map_cam_dat2, (void *)dat->addr, sizeof(map_cam_dat2));
            break;

        
        case 5:
            memcpy(&map_cam_dat3, (void *)dat->addr, sizeof(map_cam_dat3));
            break;

        case 6:
            memcpy(&map_cam_dat4, (void *)dat->addr, sizeof(map_cam_dat4));
            break;

        case 7:    
            FSpeMapDataMapping();
            map_wrk.dat_adr = GetFloorTopAddr(map_wrk.floor);
            break;

        case 8:
            motInitEnemyMdl((void *)dat->addr, dat->file_no - 799);
            break;

        case 9:
        case 10:
            motInitEnemyAnm((void *)dat->addr, dat->tmp_no, dat->file_no - 867);
            break;
        
        case 11:
            ItemLoadAfterInit(dat->file_no - 944, dat->addr);
            break;

        default:
            return;
        break;
    }

    return;
}

void DataLoadWrkInit()
{	
    int i;
    
    printf("DATA LOAD WRK INIT\n");

    memset(&load_dat_wrk, 0, sizeof(load_dat_wrk));

    for (i = 0; i < 40; i++)
    {
        load_dat_wrk[i].file_no = -1;
    }
}

void SetDataLoadWrk(MSN_LOAD_DAT* dat)
{
    int i;

    printf("SET DATA LOAD WRK\n");

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no == 0xFFFF)
        {
            load_dat_wrk[i].file_no = dat->file_no;
            load_dat_wrk[i].file_type = dat->file_type;
            load_dat_wrk[i].tmp_no = dat->tmp_no;
            load_dat_wrk[i].addr = dat->addr;
            return;
        }
    }
}

void DelDataLoadWrk(u_short file_no)
{        
    int i;

    printf("DEL DATA LOAD WRK\n");

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no == file_no)
        {
            if (file_no >= M000_MIKU_ANM && file_no <= I000_PLAY_CAMERA1_SGD)
            {
                motReleaseAniMdlBuf((u_short)file_no - M000_MIKU_ANM, (u_int *)load_dat_wrk[i].addr);
            }
            else if (load_dat_wrk[i].file_type == 2)
            {
                if (load_dat_wrk[i].addr == 16)
                {
                    ap_wrk.fg_se_empty[0] = 0;
                }
                else if (load_dat_wrk[i].addr == 17)
                {
                    ap_wrk.fg_se_empty[1] = 0;
                }
                else if (load_dat_wrk[i].addr == 18)
                {
                    ap_wrk.fg_se_empty[2] = 0;
                }
            }
            
            load_dat_wrk[i].file_no = -1;

            break;
        }
    }
}

u_int GetLoadDataAddr(u_short file_no)
{
    int i;

    printf("GET LOAD DATA ADDR\n");

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no == file_no)
        {
            return load_dat_wrk[i].addr;
        }
    }

    return 0;
}

void SortLoadDataAddr()
{
    printf("SORT LOAD DATA ADDR \n");
}

void MissionTitleDisp(int msn_no)
{	
    /* s0 16 */ int i;
	/* s3 19 */ u_char alp_rate;
	/* 0x0(sp) */ SPRT_SDAT ssd;
    SPRT_SDAT *p;
   
    printf("MISSION TITLE DISP \n");

    SetSprFile(MISSION_TITLE_CARD_ADDRESS);
    
    if (mttl_wrk.mode == 1) {
        alp_rate = ((0x1e - mttl_wrk.time) * 100) / 0x1e & 0xff;
    
    }
    else if (mttl_wrk.mode == 3 || mttl_wrk.mode == 4)
    {
        alp_rate = (mttl_wrk.time * 100) / 0x1e & 0xff;
    }
    else
        alp_rate = 100;
    


    for (i = 0; i < 11; i++) {
        SimpleDispSprt(&msn_title_sp_bak[i], MISSION_TITLE_CARD_ADDRESS, i, NULL, NULL, alp_rate);
    }



    for (i = 0; i < msn_title_flr_sp_num[msn_no]; i++) {
        SimpleDispAlphaSprt(&msn_title_sp_flr[msn_no][i], MISSION_TITLE_CARD_ADDRESS,
                            msn_title_sp_flr_no[msn_no], ((alp_rate * 0x46) / 100), 0);
    }

    for (i = 0; i < msn_title_ttl_sp_num[msn_no]; i++) {    
        SimpleDispSprt(&msn_title_sp_ttl[msn_no][i], MISSION_TITLE_CARD_ADDRESS, 
                       msn_title_sp_ttl_no[msn_no], NULL, NULL, alp_rate);    
    }
}

void StageTitleDisp(int msn_no)
{
}

void EventLoadDataInit()
{
    memset(&ev_load_wrk, 0, 8);
    return;
}

int EventLoadData(u_char load_no)
{
}

int GetLoadGhostInfo(u_char* load_inf)
{
}

void MikuCGDisp()
{
}

void RoomLoadReq(int load_room)
{
	/* s0 16 */ int i;

    printf("ROOM LOAD REQ \n");

    for (i = 0; i < 2; i++)
    {
        if (load_room == area_wrk.room[i])
        {
            return;
        }
    }
    
    for (i = 0; i < 60; i++) // Line 897
    {
        if (furn_wrk[i].room_id != room_wrk.disp_no[0]) // Line 898
        {
            if(furn_wrk[i].use == 0 || furn_wrk[i].use == 2) // Line 899 
            {
                FurnSetWrkNoUse(&furn_wrk[i], i); // Line 901
            }
        }
    } 
  
    DoorFreeFurnWrk(0); // Line 907
    FurnSortFurnWrk(0); // Line 910

    for (i = 0; i < 2; i++) // Line 914
    {
        if (plyr_wrk.pr_info.room_no != area_wrk.room[i]) // Line 915
        {
            RoomMdlLoadReq(0, i, ingame_wrk.msn_no, load_room, 2); // Line 917
            FloatGhostAppearTypeSet(ap_wrk.fgst_no, i, load_room); // Line 919
            area_wrk.room[i] = load_room;
            break;
        }
    }
}
