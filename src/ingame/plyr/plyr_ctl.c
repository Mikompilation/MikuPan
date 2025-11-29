#include "plyr_ctl.h"
#include "common.h"
#include "enums.h"
#include "ingame/entry/ap_rgost.h"
#include "main/glob.h"
#include "graphics/graph2d/effect_sub.h"
#include "ingame/enemy/ene_ctl.h"
#include "ingame/map/find_ctl.h"
#include "ingame/event/ev_main.h"
#include "graphics/graph2d/effect.h"
#include "ingame/info/inf_disp.h"

#include <math.h>

u_short photo_dmg_tbl[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110};
float circle_radius_tbl[] = {90.0f, 105.0f, 120.0f, 145.0f};
float photo_rng_tbl[] = { 5000.0 };
u_char plyr_vib_time;
static u_short hp_down_deg;
static u_char avoid_chk;
static u_char dmg_step;
static u_char no_photo_tm;
u_char charge_max_tbl[] = {6, 8, 10, 12};
u_char ini_charge_tbl[] = {0, 0, 2, 4, 6};
u_short photo_frame_tbl[][2] = {{436, 300}};

PWARP_WRK pwarp_wrk;

void PlyrCtrlMain()
{
    if ((dbg_wrk.high_speed_mode != 0) && (*key_now[6] != 0))
    {
        plyr_wrk.spd = 27.5f;
    }
    PlyrLightSet();
    PlyrSpotLightOnChk();
    if (DBG_cam_id_move_chk == 0)
    {
        PlyrBattleChk();
        PlyrTrembleChk();
        PlyrDmgChk();
        PlyrVibCtrl(0);
        PlyrHPdwonCtrl();
        if (!(plyr_wrk.sta & 0x200))
        {
            if (PlyrDoorOpenChk() == 0)
            {
                PlyrCondChk();
                PlyrTimerCtrl();
                if (ShortPauseChk() == 0)
                {
                    PlyrFinderModeChk();
                    ClrEneSta();

                    switch (plyr_wrk.mode)
                    {
                        case PMODE_NORMAL:
                            PlyrNormalCtrl();
                            break;
                        case PMODE_FINDER:
                            PlyrFinderCtrl();
                            break;
                        case PMODE_DMG:
                            PlyrDmgCtrl();
                            break;
                        case PMODE_FINDER_END:
                            PlyrFinderEnd();
                            break;
                        case PMODE_MSG_DISP:
                            PlyrMessageDisp();
                            break;
                        case PMODE_ANIME:
                            PlyrSpeAnimeCtrl();
                            break;
                        default:
                            return;
                            break;
                    }
                }
            }
        }
    }
}

int PlyrDoorOpenChk(void)
{
    if (plyr_wrk.sta & 8)
        return 1;
    return 0;
}

u_char ShortPauseChk()
{
    u_char chk;

    chk = 0;

    if (plyr_wrk.ap_timer != 0)
    {
        chk = 1;

        if ((ingame_wrk.stts & 0x10) == 0)
        {
            ingame_wrk.stts |= 0x10;
        }
    }

    else if ((ingame_wrk.stts & 0x10) != 0)
    {
        ingame_wrk.stts &= 0xef;
    }

    return chk;
}

void PlyrCondChk()
{
    if ((plyr_wrk.hp > 0 && plyr_wrk.hp <= 50) && (plyr_wrk.se_deadly == -1))
    {
        plyr_wrk.se_deadly = SeStartFix(0x24, 0, 0x1000, 0x1000, 0);
    }
    else if (plyr_wrk.se_deadly != -1)
    {
        if (plyr_wrk.hp > 50)
        {
            SeFadeFlame(plyr_wrk.se_deadly, 0x3c, 0);
            plyr_wrk.se_deadly = -1;
        }
    }

    if (plyr_wrk.cond != 0)
    {
        if (plyr_wrk.cond_tm == 0)
        {
            plyr_wrk.cond = 0;
        }
        else
        {
            plyr_wrk.cond_tm -= 1;
        }
    }
    switch (plyr_wrk.cond)
    {
        case 2:
            SetEffects(6, 1, 7, 0x14);
            if (LeverGachaChk() != 0)
            {
                plyr_wrk.cond_tm =
                    (plyr_wrk.cond_tm < 3) ? 0 : plyr_wrk.cond_tm - 2;
            }
            break;
        case 3:
            SetEffects(6, 1, 4, 0x32);
            break;
    }
    return;
}

void PlyrDmgChk()
{
    static u_char avoid_chk_tm;
    static u_char wrong_chk;

    if (plyr_wrk.dmg != 0)
    {
        dmg_step = 0;
        if (plyr_wrk.dmg_type < 2)
        {
            CallNega2(0, 0x1e, 0x3c);
            ReqPlyrHPdown(plyr_wrk.dmg, 1);
            PlyrVibCtrl(0xf);
            plyr_wrk.dmg = 0;

            if (plyr_wrk.dmg_type == 1)
            {

                dmg_step = plyr_wrk.dmg_type;
                plyr_wrk.mode = 2;
            }
        }
        else
        {
            if (avoid_chk_tm == 0)
            {
                avoid_chk = 0;
                wrong_chk = 0;
            }
            if (avoid_chk_tm < 0x12)
            {
                avoid_chk_tm += 1;
                if (avoid_chk_tm < 8)
                {
                    if (pad[0].one & 0x40 || pad[0].one & 0xF000
                        || PlyrLeverInputChk() == 2)
                    {
                        wrong_chk += 1;
                    }
                }
                else
                {
                    if (pad[0].one & 0x40 || pad[0].one & 0xF000)
                    {
                        avoid_chk = avoid_chk + 1;
                    }
                    if (PlyrLeverInputChk() == 2)
                    {
                        avoid_chk = 1;
                    }
                }
            }
            else
            {
                if ((avoid_chk != 1) || (wrong_chk != 0))
                {
                    avoid_chk = 0;
                    CallNega2(0, 0x5a, 0x3c);
                    ReqPlyrHPdown(plyr_wrk.dmg, 1);
                }
                plyr_wrk.mode = 2;
                plyr_wrk.dmg = 0;
                avoid_chk_tm = 0;
            }
        }
    }
    return;
}

void PlyrVibCtrl(u_char time)
{
    if (time != 0)
    {
        plyr_vib_time = time;
    }
    if (plyr_vib_time != 0)
    {
        VibrateRequest1(0, 1);
        plyr_vib_time -= 1;
    }
    return;
}

void PlyrFinderModeChk()
{
	u_short pad_finder;
	u_short pad_finder_bk;

    if (plyr_wrk.mode == PMODE_NORMAL)
    {
        if (opt_wrk.key_type == 0 || opt_wrk.key_type == 1 || opt_wrk.key_type == 4 || opt_wrk.key_type == 5)
        {
            pad_finder = *key_now[7];

            if (pad_finder != 0 && pad[0].push[5] < 7)
            {
                pad_finder = 0;
            }
        }
        else
        {
            pad_finder = *key_now[10];

            if (pad_finder != 0 && pad[0].push[9] < 7)
            {
                pad_finder = 0;
            }
        }
        if ((pad_finder == 1 || pad_finder == 2) &&
            (plyr_wrk.mvsta & 0x200000) == 0 &&
            plyr_wrk.cond != 1 && plyr_wrk.cond != 2 &&
            (plyr_wrk.sta & 2) == 0)
        {
            FinderInSet();
        }
    }
    else if (plyr_wrk.mode == PMODE_FINDER && (plyr_wrk.sta & 0x1000) == 0)
    {
        if (opt_wrk.key_type == 0 || opt_wrk.key_type == 1 || opt_wrk.key_type == 4 || opt_wrk.key_type == 5)
        {
            pad_finder = *key_now[7];
            pad_finder_bk = *key_bak[7];
        }
        else
        {
            pad_finder = *key_now[10];
            pad_finder_bk = *key_bak[10];
        }

        if (plyr_wrk.cond != 1)
        {
            if (pad_finder == 1 || (pad_finder_bk > 19 && pad_finder == 0))
            {
                FinderEndSet();
                plyr_wrk.mode = PMODE_FINDER_END;
            }
        }
    }
}

void FinderInSet()
{
	ENE_WRK *ew;
	sceVu0FVECTOR tv;
	float dist[2];
	u_char i;
	u_char trgt;

    plyr_wrk.mode = PMODE_FINDER_IN;

    plyr_wrk.sta |= 4;
    plyr_wrk.sta &= ~0x400;
    
    plyr_wrk.fp[0] = 0x140;
    plyr_wrk.fp[1] = 0xd1;
    
    plyr_wrk.frot_x = 0.0f;
  
    if (plyr_wrk.move_mot == 0xb) 
    {
        SetPlyrAnime(0x67,0);
    }
    else 
    {
        SetPlyrAnime(0x41,0);
    }
  
    plyr_wrk.fh_no = -1;
    PlyrChargeCtrl(0xff, NULL);

    plyr_wrk.spe1_dir = 0;
    no_photo_tm = 0x14;
    ew = &ene_wrk[0];
    dist[1] = 0.0f;
  
    for(i = 0, trgt = 0xff; i < 4; i++, ew++)
    {
        if ((ew->sta & 0x1) != 0 && ew->hp != 0 && (ew->sta & 0x80000) == 0 && (ew->sta2 & 0x1) != 0 && ew->tr_rate != 0)        
        {
            dist[0] = GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos);

            if (dist[0] <= 2500.0f)        
            {        
                if (dist[1] == 0.0f || dist[0] < dist[1])       
                {        
                    dist[1] = dist[0];        
                    trgt = i;   
                }        
            }  
        }
    }

    if (trgt != 0xff)
    {
        tv[0] = plyr_wrk.move_box.pos[0], tv[1] = plyr_wrk.move_box.pos[1], 
        tv[2] = plyr_wrk.move_box.pos[2], tv[3] = plyr_wrk.move_box.pos[3];
    
        if (plyr_wrk.move_mot == 0xb)
        {    
            tv[1] += -575.0f;
        }
        else
        {    
            tv[1] += -687.0f;
        }

        GetTrgtRot(tv, ene_wrk[trgt].mpos.p0, plyr_wrk.move_box.rspd, 3);

        plyr_wrk.move_box.rspd[1] -= plyr_wrk.move_box.rot[1];

        RotLimitChk(&plyr_wrk.move_box.rspd[1]);
        
        plyr_wrk.frot_x = plyr_wrk.move_box.rspd[0];
        plyr_wrk.move_box.rspd[0] = 0.0f;
    }
    else
    {
        plyr_wrk.move_box.rspd[0] = 0.0f, plyr_wrk.move_box.rspd[1] = 0.0f, 
        plyr_wrk.move_box.rspd[2] = 0.0f, plyr_wrk.move_box.rspd[3] = 0.0f;
    }

    for(i = 0; i < 4; i++)   
    {
        if (ene_wrk[i].sta & 1)   
        {
            ene_wrk[i].in_finder_tm = 0;
            ene_wrk[i].dist_p_e = 0;
        }    
    }

    plyr_wrk.psave[0] = plyr_wrk.move_box.pos[0], plyr_wrk.psave[1] = plyr_wrk.move_box.pos[1],
    plyr_wrk.psave[2] = plyr_wrk.move_box.pos[2], plyr_wrk.psave[3] = plyr_wrk.move_box.pos[3];
}

void FinderEndSet()
{	
    u_char i;

    plyr_wrk.sta &= 0xffffffef;
    plyr_wrk.fh_no = -1;
    plyr_wrk.mode = PMODE_NORMAL;
 
    if (plyr_wrk.move_mot == 0xb)
    {
        SetPlyrAnime(0x68,0);
    }
    else 
    {
        SetPlyrAnime(0x42,0);
    }

    for (i = 0; i < 4; i++)
    {
        if (ene_wrk[i].sta & 1)
        {            
            ene_wrk[i].in_finder_tm = 0;
            ene_wrk[i].dist_p_e = 0;
        }
    }
    SetDebugMenuSwitch(1);
}

void ClrEneSta()
{	
    u_char i;
  
    if (plyr_wrk.aphoto_tm != 0) 
    {
        plyr_wrk.aphoto_tm -= 1;
    }

    for (i = 0; i < 4; i++)
    {
        ene_wrk[i].sta &= 0xfffff89f;
    }
    return;
}

void PlyrHeightCtrl(float *tv)
{
}

void PlyrNormalCtrl(void)
{
    plyr_wrk.sta &= 0xffffffef;
    PlyrActionChk();
    PlyrNModeMoveCtrl();
    PlyrNAnimeCtrl();
    PlyrDWalkTmCtrl();
    PlyrSpotMoveCtrl();
    return;
}

void PlyrSpotMoveCtrl()
{
    sceVu0FVECTOR rv;
	float r;
	float rcng_adj;
	u_char d;
  
    if (plyr_wrk.cond == 1 || plyr_wrk.cond == 2)
    {
        return;
    }
    
    switch (plyr_wrk.mvsta & 0xFFF0) {
    case 0x10:
        r = DEG2RAD(30);
    break;
    case 0x40:
        r = DEG2RAD(45);
    break;
    default:
        r = 0;
    break;
    }
    
    rcng_adj = 0.017453292f;
    
    d = pad[0].analog[1];

    if ((plyr_wrk.mvsta & 0x200000) == 0 && (d < 0x30 || d > 0xD0))
    {
        r = -(((d - 0x80) * 0.03f) / 127.0f);
        rcng_adj = 0.05f;
        
        if (r > 0.04f)
        {                
            r = 0.04f;
        }
        
        else if (r < 0.0f)
        {
            r = 0.0f;
        }
    }

    if (plyr_wrk.spot_rot[0] < r)
    {
        if (plyr_wrk.spot_rot[0] + rcng_adj < r)
        {        
            r = plyr_wrk.spot_rot[0] + rcng_adj;
        }
    }
    else 
    {
        if (r < plyr_wrk.spot_rot[0] - rcng_adj)
        {        
            r = plyr_wrk.spot_rot[0] - rcng_adj;
        }
    }

    plyr_wrk.spot_rot[0] = r;
    rcng_adj = 0.06f;
    d = pad[0].analog[0];

    if (d >= 0x30 && d <= 0xD0)
    {
        r = 0.0f;
    }
    else if ((plyr_wrk.mvsta & 0x200000) == 0)
    {            
        r = plyr_wrk.spot_rot[0];

        r = ((d - 0x80) * 0.07f) / 127.0f;

        rcng_adj = 0.09f;
        
        if (r > 0.08f)
        {
            r = 0.08f;
        }
        else if (r < 0.010f)
        {
            r = 0.010f;
        }

        GetTrgtRot(plyr_wrk.move_box.pos, camera.p, rv, 2);

        rv[1] -= plyr_wrk.move_box.rot[1];

        RotLimitChk(&rv[1]);

        if (__builtin_fabsf(rv[1]) <= 0.011f)
        {
            r = -r;
        }
    }

    if (plyr_wrk.spot_rot[1] < r)
    {
        if (plyr_wrk.spot_rot[1] + rcng_adj < r)
        {        
            r = plyr_wrk.spot_rot[1] + rcng_adj;
        }
    }
    else 
    {
        if (r < plyr_wrk.spot_rot[1] - rcng_adj)
        {        
            r = plyr_wrk.spot_rot[1] - rcng_adj;
        }
    }

    plyr_wrk.spot_rot[1] = r;
}


void PlyrFinderCtrl() 
{
    if (!(plyr_wrk.sta & 0x1000)) 
    {
        plyr_wrk.sta |= 0x10;
        FModeScreenEffect();
        EneFrameHitChk();
        PlyrPhotoChk();
        PlyrSubAtkChk();
        PlyrCamRotCngChk();
        PlyrFModeMoveCtrl();
        PlyrDWalkTmCtrl();
        EneHPchk();
    }
}

void PlyrCamRotCngChk()
{
    if (*key_now[4] == 1 && (plyr_wrk.sta & (0x8000000 | 0x1000 | 0x400)) == 0)
    {
        plyr_wrk.sta |= 0x8000000;
        plyr_wrk.move_box.loop = 30;
        plyr_wrk.move_box.rspd[1] = DEG2RAD(6.0f);
    }
}

void EneHPchk()
{
    ENE_WRK *ew;
    float dist[2];
    u_char i;
    u_char no;

    dist[1] = 0.0f;
    no = 0xFF;
    ew = (ENE_WRK*)&ene_wrk;
    
    for (i = 0; i < 3; i++, ew++)
    {
        if ((ew->sta & 1) && ew->hp != 0 && (ew->sta & 0x40) == 0x40 && (ew->sta & 0x80000) == 0)
        {
            dist[0] = GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos);
            if (dist[1] == 0.0f || dist[0] < dist[1]) 
            {
                dist[1] = dist[0];
                no = i;
            }     
        }
    }
    
    plyr_wrk.near_ene_no = no;
}

void FModeScreenEffect()
{
	ENE_WRK *ew;
	float dist[2];
	float alpha;
	u_char i;
	u_char crate;

    for (i = 0; i < 3; i++)
    {
        if (req_dmg_ef[i])
        {
            SetEffects(9, 1, 0, 0x80000);
            return;
        }
    }

    dist[1] = 0.0f;

    ew = ene_wrk;

    for (i = 0; i < 4; i++) {
        if (ew->sta & 0x1 && ew->hp != 0 && (ew->sta & 0x80000) == 0)
        {
            dist[0] = GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos);

            if (dist[1] == 0.0f || dist[0] < dist[1])
            {
                dist[1] = dist[0]; 
            }
        }

        ew++;
    }

    alpha = 53.0f;
    crate = 70;

    if (dist[1] != 0.0f)
    {
        if (dist[1] <= 500.0f)
        {
            alpha = 80.0f;
            crate = 120;
        }
        else if (dist[1] <= 1250.0f)
        {
            alpha = (1250.0f - dist[1]) * 27.0f / 750.0f + 53.0f;
            crate = (int)((1250.0f - dist[1]) * 50.0f / 750.0f) + 70;
        }
    }

    SetEffects(2, 1, 3, alpha, 8.0f, 101, 64);
    SetEffects(14, 1, crate, crate);
}

void PlyrDmgCtrl()
{
    ENE_WRK *ew = &ene_wrk[plyr_wrk.atk_no];
    sceVu0FVECTOR tv;
    u_char n;

    switch (dmg_step) 
    {
        case 0:
            if (avoid_chk != 0) 
            {
                plyr_wrk.dwalk_tm = 0;
                ew->atk_tm = 0;
            }
            else
            {
                n = LeverGachaChk();
        
                if (ew->sta & 0x8000 && ew->atk_tm)
                {
                    if (n != 0 && ew->atk_tm >= 0x17) 
                    {
                        ew->atk_tm -= 3;
                    }
                    
                    VibrateRequest1(0, 1);
                    
                    if (ew->atk_tm > 0x14) 
                    {
                        CallDeform2(0, 0, ew->atk_tm, 7, 0xd);
                    }
                }
            }

            if (ew->atk_tm == 0) 
            {
                switch (plyr_wrk.anime_no)
                {
                case 0x3a:
                    SetPlyrAnime(0x3b, 0xa);
                    dmg_step = 1;
                break;
                case 0x3c:
                    SetPlyrAnime(0x3d, 0xa);
                    dmg_step = 1;
                break;
                default:
                    SetPlyrAnime(0x39, 0xa);
                    dmg_step = 1;
                break;
                }
                
            }
        break;
        case 1:
            if (plyr_wrk.sta & 0x20)     
            {
                plyr_wrk.mode = 0;
                dmg_step = 0;
            }
            else if (plyr_wrk.spd)
            {
                tv[0] = 0.0f;     
                tv[1] = 0.0f;    
                tv[2] = plyr_wrk.spd;    
                tv[3] = 0.0f;    
                RotFvector(plyr_wrk.move_box.rot, tv);    
                PlyrMoveHitChk(&plyr_wrk.move_box, tv, 1);    
                PlyrPosSet(&plyr_wrk.move_box, tv);    
            }
        break;
    }
}

u_char LeverGachaChk()
{
	u_char result;
	static u_char lever_dir_old;

    result = 0;

    if (PlyrLeverInputChk() == 2) 
    {
        if (lever_dir_old == pad[0].an_dir[0]) 
        {
            result = 0;
        }
        else 
        {
            lever_dir_old = pad[0].an_dir[0];
            result = 1;
        }
    }

    return result;
}

void PlyrFinderEnd(void)
{
    if (((plyr_wrk.sta & 0x20) != 0) || (plyr_wrk.anime_no != PANI_CAM_SET_OUT))
    {
        plyr_wrk.mode = PMODE_NORMAL;
    }
    return;
}

void PlyrNAnimeCtrl()
{
	u_int psta = plyr_wrk.mvsta;
    u_char anime_no = 0;
	u_char frame = 10;
    u_char rl_chk = 0;
	u_char i;
	u_char j;

	u_int psta_chk_tbl[18] = {
        0xDC,
        0xC8,
        0xB4,
        0x96,
        0x78,
        0x00,
    };

	u_char pmani_no_tbl[18][2] = {
        {0x31, 0x30},
        {0x33, 0x32},
        {0x35, 0x34},
        {0x36, 0x37}, 
        {0x46, 0x45}, 
        {0x48, 0x47}, 
        {0x4a, 0x49},
        {0x4c, 0x4b},
        {0x62, 0x62},
        {99, 99},
        {0x69, 0x69},
        {0x6a, 0x6a},
        {0x61, 0x60},
        {0x66, 0x65},
        {5, 6}, 
        {3, 4},
        {3, 4}, 
        {8, 9}};

    if (psta & 0xffff)
    {
        for (i = 0; i < 18; i++)
        {
            if (psta & psta_chk_tbl[i])
            {
                break;
            }
        }
        for (j = 0; j < 18; j++)
        {
            if ((plyr_wrk.anime_no != 49 || plyr_wrk.anime_no != 48) &&
                (pmani_no_tbl[j][0] == plyr_wrk.anime_no || pmani_no_tbl[j][1] == plyr_wrk.anime_no))
            {
                break;
            }
        }
        if (i != j)
        {
            if (j < 18)
            {
                rl_chk = (GetPlyrFtype() == 1);
            }
            
            if ((psta & 0xfff0) == 0 && j < 8)
            {
                if ((j & 1) != 0)
                {
                    rl_chk ^= 1;
                }
                frame = 20;
            }
            anime_no = pmani_no_tbl[i][rl_chk];
        }        
        else 
        {
            anime_no = plyr_wrk.anime_no; 
        }
    }

    if (anime_no == PANI_STAND)
    {
        if (psta & 0x40000)
        {
            anime_no = PANI_STAND_LOW;
        }
        
        else if (psta & 0x80000 || plyr_wrk.hp <= 50)
        {
            anime_no = PANI_DMG_STAND; // Heavy breathing animation
        }

        else if (time_wrk.no_pad >= 3600 && ingame_wrk.msn_no != 0)
        {
            anime_no = PANI_STAND2;
            frame = 60;
        }
            
        else if (time_wrk.no_pad >= 1800)
        {
            anime_no = PANI_LOOK_LR;    
        }
    }

    if (plyr_wrk.cond == 2)
    {
        anime_no = PANI_DMG_WALK_L;
    }
  
    if (anime_no == 99 || anime_no == 106)
    {
        frame = 0;
    }

    if (plyr_wrk.anime_no != anime_no && (psta & 0x200000) == 0)
    {
        SetPlyrAnime(anime_no, frame);
    } 
}

void SetPlyrAnime(u_char anime_no, u_char frame)
{
    plyr_wrk.sta &= 0xffffffdf;
    plyr_wrk.anime_no = anime_no;
    ReqPlayerAnime(frame);
    return;
}

void PlyrTrembleChk()
{
    FURN_WRK *fw;
    ENE_WRK *ew;
    sceVu0FVECTOR tv;
    float dist;
    float dist_chk;
    u_int fsta;
    u_char i;
    u_char chk;
    u_char no;
    u_short mvib_time_tbl[5] = {0x19, 0x1e, 0x2d, 0x3c, 0x5a};
    u_char mvib_deg_tbl[5] = {0xDC, 0xC8, 0xB4, 0x96, 0x78};
    static u_short mvib_time0;
    static u_short mvib_time1;
    static u_char mvib_degree;

    for (ew = ene_wrk, dist = 0.0f, chk = 0, no = 0, i = 0; i < 4; i++, ew++)
    {
        if (ew->sta & 0x1 && ew->hp != 0 && (ew->sta & 0x80000) == 0
            && (ew->sta & 0x8000) == 0)
        {

            dist_chk = GetDistV2(plyr_wrk.move_box.pos, ew->move_box.pos);
            if (dist == 0.0f || dist_chk < dist)
            {

                dist = dist_chk;
                no = i;
            }
        }
    }
    if (dist >= 1.0f && dist <= 4000.0f)
    {
        chk = 1;
        if (mvib_time1 & 0xffff)
        {
            if (mvib_time1 == 18)
            {
                SeStartFix(0xf, 0, 0x1000, 0x1000, 0);
            }
            mvib_time1--;
            VibrateRequest2(0, mvib_degree);
        }
        else if (mvib_time0 == 0)
        {
            i = (dist / 500.0f);
            if (i > 4)
                i = 4;
            mvib_time0 = mvib_time_tbl[i];
            mvib_time1 = 18;
            mvib_degree = mvib_deg_tbl[i];
            if (ene_wrk[no].type == ET_AUTO)
            {
                mvib_degree /= 2;
            }
        }
        else
        {

            mvib_time0--;
        }
    }

    if (!chk)
    {
        for (dist = 0.0f, i = 0; i < 3; i++)
        {
            if (rg_dsp_wrk[i].mode != 0)
            {
                dist_chk =
                    GetDistV(plyr_wrk.move_box.pos, &rg_dsp_wrk[i].pos[0]);
                if (dist_chk <= 5000.0f)
                {
                    if (dist == 0.0f || dist_chk < dist)
                    {

                        dist = dist_chk;
                    }
                }
            }
        }

        if (dist == 0.0f)
        {
            for (dist = 0.0f, fw = furn_wrk, i = 0; i < 60; i++, fw++)
            {
                if (GetFurnHintPos(fw, tv, &fsta) != 0)
                {
                    dist_chk = GetDistV(plyr_wrk.move_box.pos, tv);
                    if (dist_chk <= 2500.0f)
                    {
                        if (dist == 0.0f || dist_chk < dist)
                        {

                            dist = dist_chk;
                        }
                    }
                }
            }
        }
        if (dist >= 1.0f && dist <= 2500.0f)
        {
            if (mvib_time1 != 0)
            {
                mvib_time1--;
                VibrateRequest2(0, mvib_degree);
            }
            else if (mvib_time0 == 0)
            {
                mvib_time1 = 13;
                mvib_time0 = 150;
                mvib_degree = 115;
            }
            else
            {

                mvib_time0--;
            }
        }
    }
}

void ReqPlyrHPdown(u_short deg, u_char prio)
{
    if (dbg_wrk.param_muteki == 0)
    {
        if ((!(plyr_wrk.sta & 0x800) || (prio != 0))
            && !(plyr_wrk.sta & 0x4000))
        {
            plyr_wrk.sta |= 0x800;
            hp_down_deg = deg;
        }
    }
    return;
}

void PlyrHPdwonCtrl(void)
{
    u_short down;

    if (plyr_wrk.sta & 0x800)
    {
        if (hp_down_deg > 2)
        {
            down = 2;
            hp_down_deg -= 2;
        }
        else
        {
            down = hp_down_deg;
            plyr_wrk.sta &= 0xfffff7ff;
        }

        plyr_wrk.hp = (plyr_wrk.hp < down) ? 0 : plyr_wrk.hp - down;

        if (plyr_wrk.hp != 0)
        {
            return;
        }

        if (plyr_wrk.hp == 0)
        {
            if (poss_item[8] > 0)
            {
                poss_item[8] -= 1;
                plyr_wrk.sta &= 0xfffff7ff;
                plyr_wrk.sta |= 0x4000;
                SeStartFix(6, 0, 0x1000, 0x1000, 0);
                hp_down_deg = 0;
            }
            else if (ingame_wrk.game == 1)
            {
                CallMissionFailed();
            }
            else
            {
                plyr_wrk.sta &= 0xfffff7ff;
                if (plyr_wrk.se_deadly != -1)
                {
                    SeFadeFlame(plyr_wrk.se_deadly, 60, 0);
                }
                SetPlyrAnime(64, 10);
                StartGameOver();
            }
        }
    }
    else if (plyr_wrk.sta & 0x4000)
    {
        plyr_wrk.hp += 5;
        if (plyr_wrk.hp >= 500)
        {
            plyr_wrk.sta &= 0xffffbfff;
            plyr_wrk.hp = 500;
        }
    }
}

void PlyrSpotLightOnChk()
{
    SetPlyrSpotLight(1);
    if ((plyr_wrk.mode == PMODE_NORMAL) && (plyr_wrk.anime_no != PANI_CAM_SET_OUT))
    {
        SetEffects(EF_RENZFLARE, 1, 4, plyr_wrk.spot_pos, plyr_wrk.move_box.rot);
    }
    return;
}

void SetPlyrSpotLight(u_char id)
{
    SPOT_WRK ts0;
    SPOT_WRK ts1;
    sceVu0FVECTOR tv;
    sceVu0FVECTOR rv;
    u_char i;

    if (id != 0)
    {
        if (plyr_wrk.mode == PMODE_FINDER)
        {
            ts0.pos[0] = camera.p[0];
            ts0.pos[1] = camera.p[1];
            ts0.pos[2] = camera.p[2];
            ts0.pos[3] = camera.p[3];

            tv[0] = 0.0f;
            tv[1] = 0.0f;
            tv[2] = -150.0f;
            tv[3] = 0.0f;

            RotFvector(plyr_wrk.move_box.rot, tv);
            sceVu0AddVector(ts1.pos, camera.p, &tv);
            sceVu0SubVector(&ts0.direction, camera.p, &camera.i);

            ts1.direction[0] = ts0.direction[0];
            ts1.direction[1] = ts0.direction[1];
            ts1.direction[2] = ts0.direction[2];
            ts1.direction[3] = ts0.direction[3];

            ts0.diffuse[0] = 0.22f;
            ts0.diffuse[1] = 0.20999999f;
            ts0.diffuse[2] = 0.2f;
            ts0.diffuse[3] = 255.0f;
            ts1.diffuse[0] = 0.07333333f;
            ts1.diffuse[1] = 0.07f;
            ts1.diffuse[2] = 0.06666667f;
            ts1.diffuse[3] = 255.0f;

            ts0.intens = 0.69999999f;
            ts1.intens = 0.4f;
        }
        else
        {
            tv[0] = 0.0f;
            tv[1] = 0.0f;
            tv[2] = -300.0f;
            tv[3] = 0.0f;

            RotFvector(plyr_wrk.move_box.rot, tv);
            sceVu0AddVector(ts0.pos, &plyr_wrk.spot_pos, &tv);

            tv[0] = 0.0f;
            tv[1] = 0.0f;
            tv[2] = -150.0f;
            tv[3] = 0.0f;

            RotFvector(plyr_wrk.move_box.rot, tv);
            sceVu0AddVector(ts1.pos, ts0.pos, &tv);

            rv[0] = plyr_wrk.spot_rot[0];
            rv[1] = plyr_wrk.spot_rot[1];
            rv[2] = plyr_wrk.spot_rot[2];
            rv[3] = plyr_wrk.spot_rot[3];
            rv[1] += plyr_wrk.move_box.rot[1];

            RotLimitChk(&rv[1]);

            ts0.direction[0] = 0.0f;
            ts0.direction[1] = -800.0f;
            ts0.direction[2] = -3000.0f;
            ts0.direction[3] = 0.0f;

            RotFvector(rv, ts0.direction);

            ts1.direction[0] = ts0.direction[0];
            ts1.direction[1] = ts0.direction[1];
            ts1.direction[2] = ts0.direction[2];
            ts1.direction[3] = ts0.direction[3];

            ts0.diffuse[0] = 0.22f;
            ts0.diffuse[1] = 0.20999999f;
            ts0.diffuse[2] = 0.2f;
            ts0.diffuse[2] = 0.2f;
            ts0.diffuse[3] = 255.0f;

            ts1.diffuse[0] = 0.176f;
            ts1.diffuse[1] = 0.168f;
            ts1.diffuse[2] = 0.16f;
            ts1.diffuse[3] = 255.0f;

            ts0.intens = 0.69999999f;
            ts1.intens = 0.4f;
        }
        sceVu0ScaleVector(&ts0.diffuse, &ts0.diffuse, 0.001f);
        sceVu0ScaleVector(&ts1.diffuse, &ts1.diffuse, 0.001f);
        ts0.power = 7000000.0f;
        ts1.power = 7000000.0f;
    }

    for (i = 0; i < 2; i++)
    {
        if (id != 0)
        {
            if (room_wrk.mylight[i].spot_num < 2)
            {
                room_wrk.mylight[i].spot_num += 2;
            }
        }
        else
        {
            if (1 < room_wrk.mylight[i].spot_num)
            {
                room_wrk.mylight[i].spot_num -= 2;
            }
        }
        room_wrk.mylight[i].spot[0] = ts0;
        room_wrk.mylight[i].spot[1] = ts1;
    };

    for (i = 0; i < 60; i++)
    {
        if (furn_wrk[i].furn_no == 0xFFFF)
            break;
        if (id != 0)
        {
            if (furn_wrk[i].mylight.spot_num < 1)
            {
                furn_wrk[i].mylight.spot_num += 1;
            }
        }
        else
        {
            if (0 < furn_wrk[i].mylight.spot_num)
            {
                furn_wrk[i].mylight.spot_num -= 1;
            }
        }
        furn_wrk[i].mylight.spot[0] = ts0;
    }

    for (i = 0; i < 4; i++)
    {
        if (ene_wrk[i].sta & 1)
        {
            if (id != 0)
            {
                if (ene_wrk[i].mylight.spot_num < 1)
                {
                    ene_wrk[i].mylight.spot_num += 1;
                }
            }
            else
            {
                if (0 < ene_wrk[i].mylight.spot_num)
                {
                    ene_wrk[i].mylight.spot_num -= 1;
                }
            }
            ene_wrk[i].mylight.spot[0] = ts0;
            ene_wrk[i].mylight.spot[0].power *= 0.0099999998f;
        }
    }
}

void PlyrSubAtkChk()
{
    u_char hit = 0xff;
    int sub_se_tbl[4] = {0xd, 0x18, 9, 0x18};

    if (*key_now[8] == 1 && cam_custom_wrk.set_sub != 0xff && poss_item[5] != 0)
    {
        if (plyr_wrk.cond != 1)
        {
            poss_item[5] -= 1;
            switch (cam_custom_wrk.set_sub)
            {
                case 0:
                    hit = PSAchk0();
                    break;
                case 1:
                    hit = PSAchk1();
                    break;
                case 2:
                    hit = PSAchk2();
                    break;
                case 3:
                    hit = PSAchk3();
                    break;
                case 4:
                    PSAchk4(0);
                    break;
            }

            if (hit != 0xff)
            {
                if (hit != 0)
                {
                    SeStartFix(sub_se_tbl[cam_custom_wrk.set_sub], 0, 0x1000, 0x1000, 0);
                }
                else
                {
                    SeStartFix(0xc, 0, 0x1000, 0x1000, 0);
                }
            }
        }
    }

    if (cam_custom_wrk.set_spe != 0xff)
    {
        if (cam_custom_wrk.set_spe == 0)
        {
            PSAchk5();
        }
        else if (cam_custom_wrk.set_spe == 1)
        {
            if (*key_now[9] != 0 || *key_now[0xb] != 0)
            {
                if (plyr_wrk.cond != 1)
                {
                    PSAchk4(1);
                }
            }
        }
    }
    return;
}

u_char PSAchk0()
{
    ENE_WRK *ew;
    u_char i;
    u_char chk;

    ew = ene_wrk;

    for (i = 0, chk = 0; i < 3; i++)
    {
        if (ew->sta & 0x1 && ew->hp != 0 && (ew->sta & 0x80000000) == 0 && (ew->sta & 0x80000) == 0 && ew->sta & 0x400
            && (ingame_wrk.msn_no != 4 || ew->dat_no != 2) && (ew->stm_slow == 0 && ew->stm_view == 0 && ew->stm_stop == 0))
        {
            EneActSet(ew, 10);
            SetCamPush(i);
            chk = 1;
        }

        ew++;
    }

    if (!chk)
    {
        SetCamPush(-1);
    }

    return chk;
}

u_char PSAchk1()
{
    ENE_WRK *ew;
    u_char i;
    u_char chk;

    for (i = 0, chk = 0, ew = ene_wrk; i < 3; i++, ew++)
    {
        if (ew->sta & 0x1 && ew->hp != 0
            && ((ew->sta & 0x80000000) == 0 && (ew->sta & 0x80000) == 0
                && ew->sta & 0x400)
            && (ingame_wrk.msn_no != 4 || ew->dat_no != 2) && ew->act_no != 10
            && (ew->stm_view == 0 && ew->stm_stop == 0))
        {
            ew->stm_slow = 600;
            SetCamSlow(i);
            chk = 1;
        }
    }
    if (!chk)
    {
        SetCamSlow(-1);
    }
    return chk;
}

u_char PSAchk2()
{
    ENE_WRK *ew;
    u_char i;
    u_char chk;

    for (i = 0, chk = 0, ew = ene_wrk; i < 3; i++, ew++)
    {
        if (ew->sta & 0x1 && ew->hp != 0 && (ew->sta & 0x80000000) == 0
            && (ew->sta & 0x80000) == 0 && (ew->sta & 0x20)
            && (ingame_wrk.msn_no != 4 || ew->dat_no != 2) && ew->act_no != 10
            && ew->stm_slow == 0 && ew->stm_stop == 0)
        {
            ew->stm_view = 600;
            ew->tr_rate = 80;
            SetCamView(i);
            chk = 1;
        }
    }

    if (!chk)
    {
        SetCamView(-1);
    }

    return chk;
}

u_char PSAchk3()
{
    ENE_WRK *ew;
    u_char i;
    u_char chk;

    for (i = 0, chk = 0, ew = ene_wrk; i < 3; i++, ew++)
    {
        if (ew->sta & 0x1 && ew->hp != 0 && (ew->sta & 0x80000000) == 0
            && (ew->sta & 0x80000) == 0 && (ew->sta & 0x400)
            && (ingame_wrk.msn_no != 4 || ew->dat_no != 2) && ew->act_no != 10
            && ew->stm_slow == 0 && ew->stm_view == 0)
        {
            ew->stm_stop = 180;
            ene_wrk[i].sta |= 0x10000000;
            SetCamStop(i);
            chk = 1;
        }
    }

    if (!chk)
    {
        SetCamStop(-1);
    }

    return chk;
}

void PSAchk4(u_char id)
{
	sceVu0FVECTOR tv;
	sceVu0FVECTOR tr;
	sceVu0FVECTOR rv;
	ENE_WRK *ew;
	float dist[2];
    float x;
	float rot;
    u_char i;
    u_char j;
    u_char trgt;
    
    if ((plyr_wrk.sta & 0x400) == 0)
    {
        for (i = 0, j = 0, dist[1] = 0.0f, trgt = 0xff, ew = ene_wrk; i < 3; i++, j++, ew++)
        {
            if (ew->sta & 0x1 && ew->hp != 0 &&
                (ew->sta & 0x80000000) == 0 && (ew->sta & 0x80000) == 0 &&
                (ingame_wrk.msn_no != 4 || ew->dat_no != 2 || id)) {
                dist[0] = GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos);
                if ((dist[1] == 0.0f) || (dist[0] < dist[1])) {
                    trgt = i;
                    dist[1] = dist[0];
                }
            }
        }
    
        if (trgt != 0xff)
        {
            ew = &ene_wrk[trgt];
            rot = SgAtan2f(-15.0f, 224.0f / SgSinf(camera.fov * 0.5f));
            GetTrgtRot(camera.p, ew->mpos.p0, tr, 3);
            tr[0] -= plyr_wrk.frot_x;
            tr[0] += rot;
            RotLimitChk(tr);
            tr[1] -= plyr_wrk.move_box.rot[1];
            RotLimitChk(&tr[1]);
            plyr_wrk.move_box.loop = GetDist(tr[0], tr[1]) / PI;
            if (plyr_wrk.move_box.loop != 0)
            {
                plyr_wrk.sta |= 0x400;
                plyr_wrk.move_box.rspd[0] = tr[0] / plyr_wrk.move_box.loop;
                plyr_wrk.move_box.rspd[1] = tr[1] / plyr_wrk.move_box.loop;
                rv[0] = 0;
                rv[1] = (fabsf(tr[1]) < fabsf(tr[0])) ? fabsf(tr[0]) : fabsf(tr[1]);
                rv[2] = (fabsf(tr[1]) < fabsf(tr[0])) ? fabsf(plyr_wrk.move_box.rspd[0]) : fabsf(plyr_wrk.move_box.rspd[1]);
                for (j = 0; ; j++)
                {
                    rv[0] += rv[2];
                    rv[2] *= 1.1f;
                    if (rv[1] < rv[0])
                    {
                        break; 
                    }
                }
                plyr_wrk.move_box.loop = j;
                if (id == 0) { SetCamSearch(trgt); }
            }
        } 
        else
        {
            if (id == 0)
            {
                SetCamSearch(-1);
            }
        }
    }
    return;
}

void PSAchk5()
{
	ENE_WRK *ew;
	sceVu0FVECTOR rv;
	float dist[2];
	u_char i;
	u_char t;
    
    plyr_wrk.spe1_dir = 0; 

    for(dist[1] = 0.0f, t = 0xff, ew = ene_wrk, i = 0; i < 4; i++, ew++)
    {
        if (ew->sta & 1 && ew->hp != 0 && (ew->sta & 0x80000) == 0)
        {
            dist[0] = GetDistV(camera.p, ew->mpos.p0); 
            if (dist[1] == 0.0f || (dist[0] < dist[1]))
            {
                dist[1] = dist[0];
                t = i;
            }
        }
    } 
    
    if (t == 0xff)
    {
        return;
    }

    ew = &ene_wrk[t];
    
    if ((ew->sta & 0x20) != 0)
    {
        return;
    }
    
    GetTrgtRot(camera.p, ew->mpos.p0, rv, 3);
    rv[1] -= plyr_wrk.move_box.rot[1];
    RotLimitChk(&rv[1]);
    rv[0] -= plyr_wrk.frot_x;
    RotLimitChk(&rv[0]);

    if (fabsf(rv[0]) < fabsf(rv[1]))
    {
        if (rv[1] > 0.0f)
        {
            plyr_wrk.spe1_dir |= 2;
        } 
        else
        {
            plyr_wrk.spe1_dir |= 8;
        }
    }

    else if (rv[0] > 0.0f)
    {
        plyr_wrk.spe1_dir |= 1;
    }

    else
    {
        plyr_wrk.spe1_dir |= 4;
    }
}


void PlyrPhotoChk()
{
	u_short pad_shutter = 0;

    // Ghidra
    int se_no; // a0

    if (no_photo_tm != 0)
    {
        no_photo_tm -= 1;
        return;
    }
  
    if (opt_wrk.key_type < 2 || opt_wrk.key_type == 4 || opt_wrk.key_type == 5)
    {
        pad_shutter = *key_now[10];
    }
    else
    {
        pad_shutter = *key_now[7];     
    }
        
    if ((*key_now[5] != 1) && (pad_shutter != 1)) 
    {
        return;
    }
  
    if (
        (photo_wrk.mode == 0 && plyr_wrk.cond != 1) 
        && (poss_item[plyr_wrk.film_no] != 0 || cam_custom_wrk.set_spe == 3)
    ) {

        PhotoFlyChk();
        PhotoDmgChk();
        PhotoPointSet();
        if ((plyr_wrk.sta & 0x8000000) != 0) 
        {
            plyr_wrk.move_box.rspd[0] = 0.0f;
            plyr_wrk.move_box.rspd[1] = 0.0f;
            plyr_wrk.sta &= 0xf7ffffff;
            plyr_wrk.move_box.rspd[2] = 0.0f;
            plyr_wrk.move_box.rspd[3] = 0.0f;
            plyr_wrk.move_box.loop = 0;
        }
      
        switch(plyr_wrk.charge_num)
        {
            case 4:
            case 5:
            case 6:
                SeStartFix(0x15,0,0x1000,0x1000,0);
                break;
            case 7:
            case 8:
            case 9:
                SeStartFix(0x16,0,0x1000,0x1000,0);
                break;
            case 10:
            case 0xb:
            case 0xc:
                SeStartFix(0x17,0,0x1000,0x1000,0);
                break;
            default:
                SeStartFix(0x14,0,0x1000,0x1000,0);
                break;
        }
    
        PlyrChargeCtrl(0xff, 0x0);
    
        if (cam_custom_wrk.set_spe != 3)
        {
            poss_item[plyr_wrk.film_no] -= 1;
        }
        plyr_wrk.ap_timer = 120;        
        plyr_wrk.aphoto_tm = 600;
    }
    else if (poss_item[plyr_wrk.film_no] == 0)
    {
        SeStartFix(5,0,0x1000,0xa00,0); // Line 2045
    }

}

void EneFrameHitChk()
{
	ENE_WRK *ew;
	sceVu0FVECTOR tv;
	sceVu0FVECTOR rv;
	PP_JUDGE ppj;
	float dpe;
	float dce;
	float tx;
	float ty;
	u_char i;
	u_char chk;
	u_char chk2;

    chk2 = 0;
    chk = 0;
    ew = ene_wrk;

    for (i = 0; i < 4; i++, ew++)
    {
        if (ew->sta & 1 && ew->hp != 0 && (ew->sta & 0x80000) == 0)
        {
            chk2 = 1;

            dpe = ew->dist_p_e = GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos);

            if (
                OutSightChk(ew->mpos.p0, camera.p, plyr_wrk.move_box.rot[1], 
                    DEG2RAD(120.0f), photo_rng_tbl[plyr_wrk.cam_type]) == 0 && FrameInsideChk(ew->mpos.p0, &tx, &ty))
            {
                ene_wrk[i].sta |= 0x20;

                chk = 1;    

                if (ew->in_finder_tm < 0xffff)
                {
                    ew->in_finder_tm++;
                }

                ew->fp[0] = tx;
                ew->fp[1] = ty;

                dce = GetDist(tx -  plyr_wrk.fp[0], ty - plyr_wrk.fp[1]);

                if (dce <= circle_radius_tbl[cam_custom_wrk.charge_range])  
                {
                    ene_wrk[i].sta |= 0x40;
                    ew->dist_c_e = dce;  
                      
                    if (ew->dist_c_e <= 25.0f)
                    {
                        ene_wrk[i].sta |= 0x100;
                    }
                }

                if ((ew->sta & 0x1000000) == 0)
                {
                    ppj.num = 1;
                    ppj.result[0] = 0;

                    ppj.p[0][0] = ew->mpos.p0[0];
                    ppj.p[0][1] = ew->mpos.p0[1];
                    ppj.p[0][2] = ew->mpos.p0[2];
                    ppj.p[0][3] = ew->mpos.p0[3];

                    tv[2] = 200.0f;
                    tv[0] = 0.0f;
                    tv[1] = 0.0f;
                    tv[3] = 0.0f;

                    GetTrgtRot(ppj.p[0], camera.p, rv, 3);
                    RotFvector(rv, tv);

                    sceVu0AddVector(ppj.p[0], ppj.p[0], tv);

                    CheckPointDepth(&ppj);

                    if (ppj.result[0] != 0 || dpe <= 300.f)
                    {
                          ene_wrk[i].sta |= 0x200;
                    }
                }

                if (ew->sta & 0x40 || (dpe <= 510.0f && ew->sta & 0x20))
                {
                    if (ew->sta & 0x200 && ew->tr_rate != 0)
                    {
                        ene_wrk[i].sta |= 0x400;

                        if (ew->type == 0 || ew->type == 1) 
                        {
                            PlyrChargeCtrl(1, ew);
                        }
                        else 
                        {
                            plyr_wrk.charge_rate = 3.0f;
                        }
                    }  
                }
            }
            else
            {
                ew->dist_p_e = 0.0f;
                ew->in_finder_tm = 0;
            }
        }
    }

    if (chk == 0 || chk2 == 0)
    {
        PlyrChargeCtrl(0, NULL);
    }
}

void PlyrChargeCtrl(/* a0 4 */ u_char req, /* a1 5 */ ENE_WRK *ew)
{
	/* 0x0(sp) */ u_short ctime[] = { 72, 70, 60, 50 };
    /* 0x10(sp) */ float cldown[] = { 60.0f, 70.0f, 80.0f, 100.0f };
	/* f0 38 */ float dpe;
	/* f0 38 */ float rate0;
	/* f2 40 */ float rate1;
	/* a2 6 */ u_char cn = plyr_wrk.charge_num;
	/* t0 8 */ u_char cl_max;

    switch (req)
    {
    case 0xFF:
        plyr_wrk.charge_deg = 0.0f;
        plyr_wrk.charge_rate = 0.0f;
        plyr_wrk.charge_num = ini_charge_tbl[plyr_wrk.film_no];

        if (cam_custom_wrk.set_spe == 4)
        {
            plyr_wrk.charge_num = 0xc;
        }
    break;
    case 0:
        cn = ini_charge_tbl[plyr_wrk.film_no];

        if (cam_custom_wrk.set_spe == 4)
        {
            cn = 12;
        }

        plyr_wrk.charge_rate = 0.0f;

        if (plyr_wrk.charge_num == cn)
        {
            plyr_wrk.charge_deg = 0.0f;
        }
        else
        {
            if (plyr_wrk.charge_deg == plyr_wrk.charge_rate)
            {
                plyr_wrk.charge_deg = cldown[cam_custom_wrk.charge_speed];
                plyr_wrk.charge_num -= 1;
                break;
            }

            if (!(plyr_wrk.charge_deg >= 1.0f && plyr_wrk.charge_deg <= cldown[cam_custom_wrk.charge_speed]))
            {
                plyr_wrk.charge_deg = cldown[cam_custom_wrk.charge_speed];
            }

            plyr_wrk.charge_deg -= 1.0f;
        }
    break;
    default:
        dpe = ew->dist_p_e;

        if (plyr_wrk.film_no == 0)
        {
            cl_max = 1;
        }
        else
        {
            cl_max = charge_max_tbl[cam_custom_wrk.charge_max];
        }

        if (cn >= cl_max)
        {
            plyr_wrk.charge_rate = 4.5f;
        }
        else
        {
            if ((u_short)plyr_wrk.charge_deg >= ctime[cam_custom_wrk.charge_speed])
            {
                plyr_wrk.charge_deg = 0.0f;
                plyr_wrk.charge_num += 1;

                if (plyr_wrk.charge_num >= cl_max)
                {
                    plyr_wrk.charge_rate = 4.5f;

                    SeStartFix(0x13, 0, 0x1000, 0x1000, 0);
                }
                else
                {
                    SeStartFix(0x12, 0, 0x1000, 0x1000, 0);
                }
            }
            else
            {
                rate0 = 3.0f;
                rate1 = 0.8f;

                if (dpe <= 2000.0f)
                {
                    rate1 = ((2000.0f - dpe) * 0.7f) / 2000.0f + rate1;
                }

                plyr_wrk.charge_rate = rate1 * rate0;
                plyr_wrk.charge_deg += plyr_wrk.charge_rate;
            }
        }
        break;
    }
}

void PhotoFlyChk(void) {
    FLY_WRK* fw;
	float tx;
	float ty;
	u_char i;
    
    // Temp Var
    float dist;
    
    for (i = 0, fw = fly_wrk; i < 10; i++, fw++) {
        if (!(fw->sta & 1)) continue;
        if (!(fw->sta & 8)) continue;
        dist = GetDistV(plyr_wrk.move_box.pos, fw->move_box.pos);
        if (dist <= photo_rng_tbl[plyr_wrk.cam_type]) {
            if (!OutSightChk(fw->move_box.pos, camera.p,plyr_wrk.move_box.rot[1], DEG2RAD(120.0f), 5000.0f) &&
                FrameInsideChk(fw->move_box.pos, &tx, &ty)) {
                fw->sta |= 2;
            }
        }
    }
}

int FrameInsideChk(/* a0 4 */ float *tv, /* s1 17 */ float *tx, /* s0 16 */ float *ty)
{
	/* f2 40 */ float minx;
	/* f4 42 */ float maxx;
	/* f3 41 */ float miny;
	/* f0 38 */ float maxy;
    /* s2 18 */ int result = 0;

    // Nasty divisionsies by 0 precious
    GetCamI2DPos(tv, tx, ty);
    ty[0] *= 2.0f;

    minx = plyr_wrk.fp[0] - photo_frame_tbl[plyr_wrk.cam_type][0] * 0.5f;
    maxx = plyr_wrk.fp[0] + photo_frame_tbl[plyr_wrk.cam_type][0] * 0.5f; 
    miny = plyr_wrk.fp[1] - photo_frame_tbl[plyr_wrk.cam_type][1] * 0.5f;
    maxy = plyr_wrk.fp[1] + photo_frame_tbl[plyr_wrk.cam_type][1] * 0.5f;
    
    if ((minx <= tx[0]) && (tx[0] <= maxx) && (miny <= ty[0]) && (ty[0] <= maxy)) {
        result = 1;
    }
    
    return result;
}

int FrameInsideChkFurn(FURN_WRK *fw, float *degree, u_int fsta)
{
}

int FrameInsideChkRare(int wrk_no, float *degree)
{
}

u_short PhotoDmgChk() 
{
	ENE_WRK *ew;
	u_short dmg;
    int i;

    plyr_wrk.sta &= ~0x100;
    ew = ene_wrk;
    dmg = 0;
    for (i = 0; i < 4; i++, ew++)
    {
        if (ew->sta & 0x400) 
        {
            if (ew->type == 2) 
            {
                // don't ask me why these 2 don't use ew
                ene_wrk[i].sta |=  0x800;
                ene_wrk[i].sta |=  0x80000;
            } 
            else 
            {
                dmg += PhotoDmgChkSub(ew);
            }
        }
    }
    
    if (dmg != 0) 
    {
        point_get_end = 0;
    }
    
    return dmg;
}

u_short PhotoDmgChkSub(ENE_WRK *ew) 
{
	u_int i;
	u_char film_up_tbl[5] = {0, 0, 2, 3, 4};
    
    i = plyr_wrk.charge_num;
    if ((ew->sta & 0x20000)) 
    {
        i += 3;
    }

    if ((ew->sta & 0x100)) {
        i += 2;
    }

    i += film_up_tbl[plyr_wrk.film_no];
        
    if (i >= 0x16)
    {
        i = 0x15;
    }
    
    ew->dmg = photo_dmg_tbl[i];
    if (ew->hp <= ew->dmg) 
    {
        plyr_wrk.sta |= 0x100;
        if (ingame_wrk.ghost_cnt < 50000) 
        {
            ingame_wrk.ghost_cnt += 1;
        }
    }
    
    ene_wrk[(ew->move_box).idx].sta &= ~0x800;
  
    if ((ew->sta & 0x20000))
    {
        ene_wrk[(ew->move_box).idx].sta |= 0x800;
    
        if (charge_max_tbl[cam_custom_wrk.charge_max] == plyr_wrk.charge_num) 
        {
            ew->dmg_type = 3;
        }
        else 
        {
            ew->dmg_type = 2;
        }
    }
    
    else 
    {
        ew->dmg_type = 0;
    }
    return ew->dmg;
}

void PhotoPointSet()
{
    InitPhotoWrk();
    PhotoPointChkEne();
    PhotoPointChkFurn();
    PhotoPointChkRare();
    photo_wrk.room = plyr_wrk.pr_info.room_no;
}

void PhotoPointChkEne()
{
	u_char i;
	u_char cnt;
    
    for (cnt = 0, i = 0; i < 4; i++)
    {
        if (ene_wrk[i].sta & 0x400)
        {
            PhotoPointCulcEne(&ene_wrk[i], &photo_wrk);
            cnt += 1;
        }
    }
    
    if (cnt >= 3)
    {
        photo_wrk.bonus_sta |= 0x10;
    }

    else if (cnt >= 2)
    {
        photo_wrk.bonus_sta |= 0x8;
    }
}

void PhotoPointChkFurn()
{
    PHOTO_WRK *pw;
	FURN_WRK *fw;
	sceVu0FVECTOR tv;
	u_int sta;
	u_int i;
	float dist;
	float degree;

    pw = &photo_wrk;
    
    if (plyr_wrk.fh_no != 0xFFFF)
    {
        fw = &furn_wrk[plyr_wrk.fh_no];
        dist = GetDistV(fw->pos, plyr_wrk.move_box.pos);
        sta = GetFurnAttrF(fw, ingame_wrk.msn_no);
        PhotoPointCulcFurn(fw, pw, dist, plyr_wrk.fh_deg, sta);
    }
    
    for (i = 0, fw = furn_wrk; i < 60; i++, fw++) 
    {
        if (GetFurnHintPos(fw, tv, &sta) == 0) 
        {
            sta = GetFurnAttrF(fw, ingame_wrk.msn_no);
            if ((sta & 0x80) && ((fw->furn_no < 1000 != 0) || (fw->furn_no == 0xFFFF)))
            {
                dist = GetDistV(fw->pos, plyr_wrk.move_box.pos);
                if ((furn_dat[fw->id].dist_n <= dist) && (dist <= furn_dat[fw->id].dist_f) 
                    && (FrameInsideChkFurn(fw, &degree, sta) != 0))
                {
                    PhotoPointCulcFurn(fw, pw, dist, degree, sta);
                }
            }
        }
    }
}

void PhotoPointChkRare()
{
	/* s1 17 */ u_int i;
	/* 0x0(sp) */ float degree;

    for (i = 0; i < 3; i++)
    {
        if ((rg_dsp_wrk[i].mode != 0) && (rg_dsp_wrk[i].alpha >= 0x64) 
            && (FrameInsideChkRare(i, &degree) != 0))
        {
            PhotoPointCulcRare(i, &photo_wrk, GetDistV(rg_dsp_wrk[i].pos, plyr_wrk.move_box.pos), degree);
            RareGhostDispEndSet(i);
        }
    }
}


void PhotoPointCulcEne(ENE_WRK *ew, PHOTO_WRK *pw)
{
}

void PhotoPointCulcFurn(FURN_WRK *fw, PHOTO_WRK *pw, float dist, float degree, u_int stts)
{
}

void PhotoPointCulcRare(u_char wrk_no, PHOTO_WRK *pw, float dist, float degree)
{
}

void PlyrMpRecoverChk(u_int recov)
{
}

void PlyrFModeMoveCtrl()
{
}

void PlyrActionChk()
{
    if (*key_now[5] == 1)
    {
        DoorCheckOn(0);
        return;
    }
    return;
}

void PlyrNModeMoveCtrl()
{
	sceVu0FVECTOR tv;
	MOVE_BOX *mb;
  
    if (plyr_wrk.cond != 1)
    {
        mb = &plyr_wrk.move_box;

        if (plyr_wrk.cond == 2)
        {
            PlyrKonwakuMove(mb, tv);
        }
        else if (opt_wrk.key_type == 1 || opt_wrk.key_type == 3 || opt_wrk.key_type == 5 || opt_wrk.key_type == 7)
        {
            PlyrMovePadV(mb, tv);
        }
        else 
        {
            PlyrMovePad(mb, tv);
        }

        if (PlyrMoveHitChk(mb, tv, 0))
        {
            PlyrHitTurnChk(mb, tv);
        }

        PlyrPosSet(mb, tv);

        if (dbg_wrk.high_speed_mode == 0)
        {
            PlyrSpecialMoveChk(tv);
        }
    }
}

void PlyrHitTurnChk(MOVE_BOX *mb, float *tv)
{
}

u_char PlyrSpecialMoveChk2(float *mv)
{
}

void PlyrSpecialMoveChk(float *mv)
{
}

void PlyrPosSet(MOVE_BOX *mb,float *tv)
{  
    if (plyr_wrk.pr_info.room_old != plyr_wrk.pr_info.room_no)
    {
        plyr_wrk.dop.dov[0] = plyr_wrk.op[0];
        plyr_wrk.dop.dov[1] = plyr_wrk.op[1];
        plyr_wrk.dop.dov[2] = plyr_wrk.op[2];
        plyr_wrk.dop.dov[3] = plyr_wrk.op[3];
        plyr_wrk.dop.room_no = plyr_wrk.pr_info.room_old;
    }

    plyr_wrk.op[0] = mb->pos[0];
    plyr_wrk.op[1] = mb->pos[1];
    plyr_wrk.op[2] = mb->pos[2];
    plyr_wrk.op[3] = mb->pos[3];
    plyr_wrk.mv[0] = *tv;
    plyr_wrk.mv[1] = tv[1];
    plyr_wrk.mv[2] = tv[2];
    plyr_wrk.mv[3] = tv[3];
  
    sceVu0AddVector(mb->pos,mb->pos,tv);
  
    if ((plyr_wrk.mvsta & 0x200000) != 0)
    {
        mb->pos[1] += plyr_wrk.spd_ud;
        return;
    }
  
    PlyrHeightCtrl(tv);
    return;
}

void PlyrKonwakuMove(MOVE_BOX *mb, float *tv)
{
    tv[0] = 0.0;
    tv[1] = 0.0;
    tv[2] = 2.0;
    tv[3] = 0.0;
    RotFvector(mb->rot, tv);
    return;
}

void PlyrMovePad(MOVE_BOX *mb, float *tv)
{
}

void GetMoveSpeed(float *tv)
{
}

void PlyrMovePadV(MOVE_BOX *mb, float *tv)
{
}

void CngPlyrRotRapid(float rot0)
{
}

void PlyrMovePadFind(float *tv)
{
}

float GetMovePad(u_char id)
{
}

int MovePadEnableChk(u_char *dir_save)
{
}

void PadInfoTmpSave(u_char *dir_save, u_char dir_now, float *rot_save,
                    float rot_now)
{
}

u_char PlyrMoveStaChk(float pad_chk)
{
}

u_int PlyrLeverInputChk()
{
}

u_char PlyrMoveHitChk(MOVE_BOX *mb, float *tv, u_char id)
{
    float dist;
    u_char result = 0;
    u_char div;

    if (tv[0] != 0.0f || tv[2] != 0.0f)
    {
        dist = GetDist(*tv,tv[2]);   
     
        if (24.0f < dist)
        {
            div = (dist / 24.0f);
        }
     
        else
        {
            div = dist;
            if ((div & 0xffU) == 0)
            {
                div = 1;
            }
        }
    
        result = PlyrMapHitCheck(tv,mb->pos,div,plyr_wrk.pr_info.room_no);
    
        if (id != 0)
        {
            PlyrSpecialMoveChk2(tv);
        }    
    }
    return result;
}

void InitPhotoWrk()
{
	/* s2 18 */ PHOTO_WRK *pw;
	/* s0 16 */ int i;

    pw = &photo_wrk;
    
    memset(pw, 0, sizeof(PHOTO_WRK));
    
    InitSubjectWrk(&pw->plyr);
    

    for (i = 0; i <= 3; i++)
    {
        InitSubjectWrk(&pw->ene[i]);
    } 
 
    for(i = 0; i <= 4; i++)
    {
        InitSubjectWrk(&pw->furn[i]);
    }
      
    for (i = 0; i <= 2; i++) 
    {
        InitSubjectWrk(&pw->rare[i]);
    }

    pw->mode = 1;
    pw->dist_2d = -1;
    pw->ene_dead = 0xff;
    pw->ratio = 1.0f;
}

void InitSubjectWrk(SUBJECT_WRK *sw)
{
    sw->no = -1;
    sw->ratio = 1.0f;
    return;
}

void PlyrBattleChk()
{
    ENE_WRK *ew;
    int i;

    plyr_wrk.sta &= ~0x1;

    for (i = 0, ew = ene_wrk; i <= EWRK_GHOST2; i++, ew++)
    {
        if ((ew->sta & 1))
        {
            if (GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos) <= 5000.0f
                || plyr_wrk.pr_info.room_no == ew->room_no)
            {
                plyr_wrk.sta |= 1;
                break;
            }
        }
    }
    return;
}

void PlyrLightSet()
{
}

void PlyrMessageDisp(void)
{
    if (find_wrk.mode != 0 && FindMapCtrl() != 0)
    {
        plyr_wrk.mode = PMODE_NORMAL;
        return;
    } 
    if (ingame_wrk.mode == INGAME_MODE_GET_ITEM)
    {
        if (ItemGetCtrl() != 0)
        {
            ev_wrk.get_item = 0;
            plyr_wrk.mode = PMODE_NORMAL;   
            ingame_wrk.mode = INGAME_MODE_NOMAL;
            eff_blur_off = 0;
        }
    } 
    else if (ev_wrk.btl_lock != 0)
    {
        LockBattleDoorOpenMSGDisp();
    }
}

int ReqPlyrSpeAnime(u_char anime_no, u_char frame)
{
	int result = 0;

    if (plyr_wrk.mode == PMODE_NORMAL)
    {   
        if (!(plyr_wrk.mvsta & 0x200000) && !(plyr_wrk.sta & 8) && !(plyr_wrk.sta & 1))
        {
            if (anime_no != plyr_wrk.anime_no)
            {
                SetPlyrAnime(anime_no, frame);
                result = 1;
            }
        }
    }
    return result;
}

void PlyrSpeAnimeCtrl()
{
	MOVE_BOX *mb = &plyr_wrk.move_box;
	sceVu0FVECTOR tv;
    
    if (plyr_wrk.sta & 0x20)
    {
        plyr_wrk.mode = PMODE_NORMAL;
    }
    else
    {
        tv[0] = 0.0f;
        tv[2] = -plyr_wrk.spd;
        tv[1] = 0.0f;
        tv[3] = 0.0f;   
        RotFvector(plyr_wrk.move_box.rot, tv);
        PlyrMoveHitChk(mb, tv, 1);
        PlyrPosSet(mb, tv); 
    }    
    return;
}

void PlyrDWalkTmCtrl()
{
    if (plyr_wrk.dwalk_tm != 0)
    {
        plyr_wrk.dwalk_tm -= 1;
    }
}


float GetEnePowerDegree()
{
}

float CulcEP(float *v0, float *v1)
{
}

float CulcEP2(float *v0, float *v1)
{
}

int ChkPhotoAble()
{
}

int SearchRareEne()
{
}

int SearchFurnHint()
{
}

int GetFurnHintPos(FURN_WRK *fw, float *tv, u_int *fsta)
{
}

float GetPlyrSpd()
{
}

int PlyrNeckDirectionChk(float *p)
{
}

u_char NeckTargetEneChk(float *p)
{
}

u_char NeckTargetItemChk(float *p)
{
}

u_char NeckTargetDoorChk(float *p)
{
}

u_char PlyrNoticeObjectChk(float *ov, float *dist)
{
}

void ReqPlayerStop(u_char frame)
{
}

void PlayerWarpReq(u_char dat_no)
{
}

int PlayerWarpCtrl()
{
}

void PlayerWarpRoomLoadReq()
{
}

int ShutterChanceChk()
{
}

void PlayerWarpReq2(u_char dat_no)
{
    pwarp_wrk.req_no = dat_no;
    ingame_wrk.stts |= 0x80;
    pwarp_wrk.time = 0x20;
    pwarp_wrk.mode = PWARP_MODE_IN_REQ;
    SetPanel(0x10, 0.0f, 0.0f, 640.0f, 448.0f, 0, 0, 0, 0x80);
    SetBlackOut2(1);
}
