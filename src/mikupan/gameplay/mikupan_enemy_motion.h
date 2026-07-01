#ifndef MIKUPAN_ENEMY_MOTION_H
#define MIKUPAN_ENEMY_MOTION_H

#include "typedefs.h"

#include "graphics/graph3d/gra3d.h"
#include "graphics/motion/ani_dat.h"
#include "graphics/motion/motion.h"
#include "main/glob.h"
#include "mikupan/debug/mikupan_assert.h"
#include "mikupan/debug/mikupan_logging_c.h"

static inline int MikuPan_MotEnemyModelResident(u_int mdl_no)
{
    if (mdl_no >= 70)
    {
        return 0;
    }

    return pmanmodel[mdl_no] != NULL;
}

static inline int MikuPan_MotEnemyAnmResident(u_int anm_no)
{
    u_int i;

    for (i = 1; i < 20; i++)
    {
        if (ani_mdl_ctrl[i].anm_no == anm_no && ani_mdl_ctrl[i].anm_p != NULL)
        {
            return 1;
        }
    }

    return 0;
}

static inline void MikuPan_MotDumpAniMdlCtrl(const char *reason)
{
    u_int i;

    critical_log("ANI_MDL_CTRL dump: %s", reason ? reason : "(no reason)");

    for (i = 0; i < 20; i++)
    {
        critical_log("  ani_mdl_ctrl[%u]: map=%u anm_no=%u pkt_no=%u anm_p=%p ani.mdl_p=%p",
            i,
            ani_mdl_ctrl[i].map_flg,
            ani_mdl_ctrl[i].anm_no,
            ani_mdl_ctrl[i].pkt_no,
            ani_mdl_ctrl[i].anm_p,
            ani_mdl[i].mdl_p);
    }

    MikuPan_FlushLog();
}

static inline int MikuPan_MotCheckSetEneTextureInputs(u_int work_id)
{
    MIKUPAN_ASSERT_MSG(work_id < 4, "SetEneTexture invalid work_id=%u", work_id);
    if (work_id >= 4)
    {
        return 0;
    }

    MIKUPAN_ASSERT_MSG(ene_wrk[work_id].dat != NULL,
        "SetEneTexture missing ene_wrk[%u].dat type=%u dat_no=%u sta=0x%x",
        work_id,
        ene_wrk[work_id].type,
        ene_wrk[work_id].dat_no,
        ene_wrk[work_id].sta);
    if (ene_wrk[work_id].dat == NULL)
    {
        return 0;
    }

    return 1;
}

static inline int MikuPan_MotValidateEnemyAniCtrl(const char *context, u_int work_id, ANI_CTRL *ani)
{
    const char *where = context ? context : "enemy animation";

    if (work_id >= 4 || ene_wrk[work_id].dat == NULL)
    {
        return 0;
    }

    MIKUPAN_ASSERT_MSG(ani != NULL,
        "%s missing ANI_CTRL work=%u type=%u dat_no=%u mdl_no=%u anm_no=%u pkt_no=%u buf_no=%u",
        where,
        work_id,
        ene_wrk[work_id].type,
        ene_wrk[work_id].dat_no,
        ene_wrk[work_id].dat->mdl_no,
        ene_wrk[work_id].dat->anm_no,
        ene_pkt_ctrl[work_id].pkt_no,
        ene_pkt_ctrl[work_id].buf_no);
    if (ani == NULL)
    {
        MikuPan_MotDumpAniMdlCtrl(where);
        return 0;
    }

    MIKUPAN_ASSERT_MSG(ani->mdl_p != NULL,
        "%s null mdl_p work=%u type=%u dat_no=%u mdl_no=%u anm_no=%u pkt_no=%u buf_no=%u",
        where,
        work_id,
        ene_wrk[work_id].type,
        ene_wrk[work_id].dat_no,
        ene_wrk[work_id].dat->mdl_no,
        ene_wrk[work_id].dat->anm_no,
        ene_pkt_ctrl[work_id].pkt_no,
        ene_pkt_ctrl[work_id].buf_no);
    if (ani->mdl_p == NULL)
    {
        MikuPan_MotDumpAniMdlCtrl(where);
        return 0;
    }

    return 1;
}

#endif // MIKUPAN_ENEMY_MOTION_H
