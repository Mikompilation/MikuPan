#ifndef MIKUPAN_ENEMY_ENTRY_H
#define MIKUPAN_ENEMY_ENTRY_H

#include "typedefs.h"
#include "enums.h"

#include "graphics/graph2d/effect_ene.h"
#include "graphics/motion/mdlwork.h"
#include "ingame/enemy/ene_ctl.h"
#include "ingame/event/ev_load.h"
#include "main/glob.h"
#include "mikupan/mikupan_assert.h"
#include "mikupan/mikupan_enemy_motion.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_memory.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/eese.h"

#ifndef SE_ADDRNO_GHOST2
#define SE_ADDRNO_GHOST2 18
#endif

typedef struct {
    u_char active;
    u_char mode;
    u_char type;
    u_char dat_no;
    u_int mdl_no;
    u_int anm_no;
    u_int se_no;
    int load_id;
} MIKUPAN_MISSING_ENEMY_LOAD;

static MIKUPAN_MISSING_ENEMY_LOAD s_mikupan_missing_enemy_load[4] = {0};

static inline ENE_DAT *MikuPan_EneEntryGetPendingDat(u_char no)
{
    static ENE_DAT aene_tmp[4];
    AENE_INFO_DAT *aie;

    if (no >= 4)
    {
        return NULL;
    }

    switch (ene_wrk[no].type)
    {
    case 0:
        return &jene_dat[ingame_wrk.msn_no][ene_wrk[no].dat_no];
    case 1:
        return &fene_dat[ingame_wrk.msn_no][ene_wrk[no].dat_no];
    case 2:
        aie = &aene_info_dat[ingame_wrk.msn_no][ene_wrk[no].dat_no];
        aene_tmp[no] = ENE_DAT{};
        aene_tmp[no].mdl_no = aie->mdl_no;
        aene_tmp[no].anm_no = aie->anm_no;
        aene_tmp[no].se_no = aie->se_no;
        aene_tmp[no].point_base = aie->point_base;
        return &aene_tmp[no];
    default:
        return NULL;
    }
}

static inline int MikuPan_EneEntryResourceResident(const ENE_DAT *dat)
{
    if (dat == NULL)
    {
        return 0;
    }

    return MikuPan_MotEnemyModelResident(dat->mdl_no) && MikuPan_MotEnemyAnmResident(dat->anm_no);
}

static inline void MikuPan_EneEntryDumpLoadDataWrk(const char *reason)
{
    int i;

    critical_log("load_dat_wrk dump: %s", reason ? reason : "(no reason)");

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no != 0xffff)
        {
            critical_log("  load_dat_wrk[%d]: file_no=%u type=%u tmp=%u addr=0x%x",
                i,
                load_dat_wrk[i].file_no,
                load_dat_wrk[i].file_type,
                load_dat_wrk[i].tmp_no,
                load_dat_wrk[i].addr);
        }
    }

    MikuPan_FlushLog();
}

static inline void MikuPan_EneEntryRegisterLoadData(u_short file_no, u_char file_type, u_char tmp_no, int addr)
{
    MSN_LOAD_DAT dat;

    dat.file_no = file_no;
    dat.file_type = file_type;
    dat.tmp_no = tmp_no;
    dat.addr = addr;

    SetDataLoadWrk(&dat);
}

static inline int MikuPan_EneEntryBeginMissingEnemyLoad(u_char no, const ENE_DAT *dat)
{
    MIKUPAN_MISSING_ENEMY_LOAD *load;

    if (no >= 4 || dat == NULL)
    {
        return 0;
    }

    load = &s_mikupan_missing_enemy_load[no];

    if (load->active)
    {
        if (load->type == ene_wrk[no].type
            && load->dat_no == ene_wrk[no].dat_no
            && load->mdl_no == dat->mdl_no
            && load->anm_no == dat->anm_no)
        {
            return 1;
        }

        warn_log("Resetting stale missing enemy load slot=%u old type=%u dat=%u mdl=%u anm=%u new type=%u dat=%u mdl=%u anm=%u",
            no,
            load->type,
            load->dat_no,
            load->mdl_no,
            load->anm_no,
            ene_wrk[no].type,
            ene_wrk[no].dat_no,
            dat->mdl_no,
            dat->anm_no);
        *load = MIKUPAN_MISSING_ENEMY_LOAD{};
    }

    warn_log("Missing enemy resource; starting on-demand load slot=%u type=%u dat_no=%u mdl_no=%u anm_no=%u se_no=%u room=%u msn=%u",
        no,
        ene_wrk[no].type,
        ene_wrk[no].dat_no,
        dat->mdl_no,
        dat->anm_no,
        dat->se_no,
        plyr_wrk.pr_info.room_no,
        ingame_wrk.msn_no);

    MikuPan_MotDumpAniMdlCtrl("missing enemy resource before on-demand load");
    MikuPan_EneEntryDumpLoadDataWrk("missing enemy resource before on-demand load");

    DelDataLoadWrk((u_short)(dat->mdl_no + M000_MIKU_MDL));
    DelDataLoadWrk((u_short)(dat->anm_no + M000_MIKU_ANM));

    if ((int)dat->se_no >= 0)
    {
        DelDataLoadWrk((u_short)dat->se_no);
    }

    *load = MIKUPAN_MISSING_ENEMY_LOAD{};
    load->active = 1;
    load->mode = 1;
    load->type = ene_wrk[no].type;
    load->dat_no = ene_wrk[no].dat_no;
    load->mdl_no = dat->mdl_no;
    load->anm_no = dat->anm_no;
    load->se_no = dat->se_no;
    load->load_id = LoadReq((int)(dat->mdl_no + M000_MIKU_MDL), MODEL_ADDRESS);

    if (load->load_id < 0)
    {
        critical_log("Missing enemy model LoadReq failed slot=%u mdl_no=%u file_no=%u", no, dat->mdl_no, dat->mdl_no + M000_MIKU_MDL);
        *load = MIKUPAN_MISSING_ENEMY_LOAD{};
        return 0;
    }

    return 1;
}

static inline int MikuPan_EneEntryMissingEnemyLoadMain(u_char no)
{
    MIKUPAN_MISSING_ENEMY_LOAD *load;

    if (no >= 4)
    {
        return 0;
    }

    load = &s_mikupan_missing_enemy_load[no];

    if (!load->active)
    {
        return 1;
    }

    switch (load->mode)
    {
    case 1:
        if (IsLoadEnd(load->load_id) == 0)
        {
            return 0;
        }

        motInitEnemyMdl((u_int *)MikuPan_GetHostPointer(MODEL_ADDRESS), load->mdl_no);
        MikuPan_EneEntryRegisterLoadData((u_short)(load->mdl_no + M000_MIKU_MDL), 8, 0, MODEL_ADDRESS);

        LoadEneDmgTex((int)load->mdl_no, (u_int *)MikuPan_GetHostPointer(ANIM_ADDRESS + ENE_DMG_TEX_BASE_ADDRESS));
        load->load_id = LoadReq((int)(load->anm_no + M000_MIKU_ANM), ANIM_ADDRESS);
        if (load->load_id < 0)
        {
            critical_log("Missing enemy animation LoadReq failed slot=%u anm_no=%u file_no=%u", no, load->anm_no, load->anm_no + M000_MIKU_ANM);
            *load = MIKUPAN_MISSING_ENEMY_LOAD{};
            return 1;
        }
        load->mode = 2;
        return 0;
    case 2:
        if (IsLoadEnd(load->load_id) == 0)
        {
            return 0;
        }

        motInitEnemyAnm((u_int *)MikuPan_GetHostPointer(ANIM_ADDRESS), load->mdl_no, load->anm_no);
        MikuPan_EneEntryRegisterLoadData((u_short)(load->anm_no + M000_MIKU_ANM), 9, (u_char)load->mdl_no, ANIM_ADDRESS);

        if ((int)load->se_no >= 0)
        {
            load->load_id = SeFileLoadAndSet(load->se_no, SE_ADDRNO_GHOST2);
            MikuPan_EneEntryRegisterLoadData((u_short)load->se_no, 2, 0, SE_ADDRNO_GHOST2);
            if (load->load_id >= 0)
            {
                load->mode = 3;
                return 0;
            }
        }

        critical_log("Missing enemy resource on-demand load finished slot=%u type=%u dat_no=%u mdl=%u anm=%u",
            no, load->type, load->dat_no, load->mdl_no, load->anm_no);
        *load = MIKUPAN_MISSING_ENEMY_LOAD{};
        return 1;
    case 3:
        if (IsLoadEnd(load->load_id) == 0)
        {
            return 0;
        }

        critical_log("Missing enemy resource on-demand load finished slot=%u type=%u dat_no=%u mdl=%u anm=%u se=%u",
            no, load->type, load->dat_no, load->mdl_no, load->anm_no, load->se_no);
        *load = MIKUPAN_MISSING_ENEMY_LOAD{};
        return 1;
    default:
        *load = MIKUPAN_MISSING_ENEMY_LOAD{};
        return 1;
    }
}

static inline void MikuPan_EneEntryCancel(u_char no, const char *reason)
{
    u_char type;
    u_char dat_no;

    if (no >= 4)
    {
        return;
    }

    type = ene_wrk[no].type;
    dat_no = ene_wrk[no].dat_no;

    critical_log("Cancelling enemy entry slot=%u type=%u dat_no=%u reason=%s", no, type, dat_no, reason ? reason : "(no reason)");
    InitEneWrk(no);
    ene_wrk[no].type = type;
    ene_wrk[no].dat_no = dat_no;
    ene_wrk[no].sta = 0;
}

static inline int MikuPan_EneEntryBeforeOriginal(u_char no)
{
    ENE_DAT *pending_dat;

    MIKUPAN_ASSERT_MSG(no < 4, "EneEntryChk invalid slot=%u", no);
    if (no >= 4)
    {
        return 0;
    }

    if (s_mikupan_missing_enemy_load[no].active)
    {
        if (MikuPan_EneEntryMissingEnemyLoadMain(no) == 0)
        {
            return 0;
        }
    }

    MIKUPAN_ASSERT_MSG(ene_wrk[no].type <= 2,
        "EneEntryChk invalid enemy type slot=%u type=%u dat_no=%u",
        no,
        ene_wrk[no].type,
        ene_wrk[no].dat_no);
    if (ene_wrk[no].type > 2)
    {
        MikuPan_EneEntryCancel(no, "invalid enemy type");
        return 0;
    }

    pending_dat = MikuPan_EneEntryGetPendingDat(no);

    if (pending_dat != NULL && !MikuPan_EneEntryResourceResident(pending_dat))
    {
        if (MikuPan_EneEntryBeginMissingEnemyLoad(no, pending_dat) != 0)
        {
            return 0;
        }

        MikuPan_EneEntryCancel(no, "failed to begin missing enemy load");
        return 0;
    }

    return 1;
}

static inline int MikuPan_EneEntryAfterAnmPacket(u_char no)
{
    ANI_CTRL *ani;

    ani = motGetAniMdl(no);

    if (!MikuPan_MotValidateEnemyAniCtrl("EneEntryChk after motSetAnmPacket", no, ani))
    {
        MikuPan_EneEntryDumpLoadDataWrk("EneEntryChk failed after motSetAnmPacket");
        MikuPan_EneEntryCancel(no, "invalid ANI_CTRL after motSetAnmPacket");
        return 0;
    }

    return 1;
}

#endif // MIKUPAN_ENEMY_ENTRY_H
