#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "outgame/mot_menu.h"
#include "outgame/mot_test.h"

#include "mikupan/mikupan_memory.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "outgame/outgame.h"

#include "sce/libvu0.h"

#include "graphics/graph2d/g2d_main.h"
#include "graphics/graph3d/gra3d.h"
#include "graphics/graph3d/object.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglight.h"
#include "graphics/graph3d/sgsu.h"
#include "graphics/motion/accessory.h"
#include "graphics/motion/mdldat.h"
#include "graphics/motion/mdlwork.h"
#include "graphics/motion/mime.h"
#include "graphics/motion/motion.h"

#include <string.h>

#define PI 3.1415927f
#define DEG2RAD(x) ((float)(x) * PI / 180.0f)

typedef enum {
    MOT_TEST_LOAD_MODEL,
    MOT_TEST_WAIT_MODEL,
    MOT_TEST_LOAD_ANIM,
    MOT_TEST_WAIT_ANIM,
    MOT_TEST_READY,
} MOT_TEST_LOAD_STEP;

typedef struct {
    u_char file_id[4];
    u_int map_flg;
    u_int bone_num;
    u_int trans_num;
    u_int frame_num;
    u_int interp_frame;
    u_int flg;
    u_int si_frame;
} MOT_TEST_MOT_HEADER;

typedef struct {
    ANI_CTRL ani;
    SgCAMERA camera;
    MOT_TEST_LOAD_STEP load_step;
    int load_id;
    MOT_MENU_WRK menu;
} MOT_TEST_WRK;

static MOT_TEST_WRK mot_test_wrk;

static void MotTestRequestLoad(void);
static void MotTestLoadCtrl(void);
static void MotTestSetAnim(void);
static void MotTestDraw3D(void);

static int MotTestAnimCount(u_int anm_no)
{
    int num;
    ANI_CODE **tbl;

    tbl = anm_tbl[anm_no];

    if (tbl == NULL)
    {
        return 0;
    }

    for (num = 0; tbl[num] != NULL; num++)
    {
        ;
    }

    return num;
}

static int MotTestFrameCount(u_int *mot_p)
{
    MOT_TEST_MOT_HEADER *mfh;

    if (mot_p == NULL)
    {
        return 1;
    }

    mfh = (MOT_TEST_MOT_HEADER *)mot_p;

    if (mfh->frame_num == 0)
    {
        return 1;
    }

    return mfh->frame_num;
}

static void MotTestSetVector(sceVu0FVECTOR v, float x, float y, float z, float w)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = w;
}

static void MotTestSetCamera(void)
{
    SgCAMERA *cam;

    cam = &mot_test_wrk.camera;
    memset(cam, 0, sizeof(*cam));

    MotTestSetVector(cam->p, 0.0f, mot_test_wrk.menu.height, mot_test_wrk.menu.zoom, 1.0f);
    MotTestSetVector(cam->i, 0.0f, mot_test_wrk.menu.height, 0.0f, 1.0f);

    cam->roll = PI;
    cam->fov = DEG2RAD(44.0f);
    cam->nearz = 0.1f;
    cam->farz = 32768.0f;
    cam->ax = 1.0f;
    cam->ay = 0.40689999f;
    cam->cx = 2048.0f;
    cam->cy = 2048.0f;
    cam->zmin = 0.0f;
    cam->zmax = 16777215.0f;

    SgSetRefCamera(cam);
}

static void MotTestSetLight(void)
{
    static SgLIGHT light[1];
    sceVu0FVECTOR ambient = {0.35f, 0.35f, 0.35f, 1.0f};

    memset(light, 0, sizeof(light));

    light[0].direction[0] = -0.35f;
    light[0].direction[1] = -0.65f;
    light[0].direction[2] = -0.55f;
    light[0].direction[3] = 0.0f;
    light[0].diffuse[0] = mot_test_wrk.menu.light_power;
    light[0].diffuse[1] = mot_test_wrk.menu.light_power;
    light[0].diffuse[2] = mot_test_wrk.menu.light_power;
    light[0].diffuse[3] = 1.0f;
    light[0].specular[0] = mot_test_wrk.menu.light_power;
    light[0].specular[1] = mot_test_wrk.menu.light_power;
    light[0].specular[2] = mot_test_wrk.menu.light_power;
    light[0].specular[3] = 1.0f;
    light[0].Enable = 1;
    light[0].num = 1;

    SgSetAmbient(ambient);
    SgSetInfiniteLights(mot_test_wrk.camera.zd, light, 1);
    SgSetPointLights(NULL, 0);
    SgSetSpotLights(NULL, 0);
}

static u_int *MotTestAniModelPointer(u_short mdl_no)
{
    if (mdl_no == M000_MIKU || (mdl_no == M001_MAFUYU && plyr_init_ctrl.msn_no == 0))
    {
        return pmanmpk[mdl_no];
    }

    return pmanmodel[mdl_no];
}

static void MotTestSetAnim(void)
{
    HeaderSection *hs;
    const MOT_MENU_ENTRY *entry;
    MOT_MENU_WRK *menu;

    menu = &mot_test_wrk.menu;
    entry = MotMenuEntry(menu->entry_no);

    menu->anim_num = MotTestAnimCount(entry->anm_no);

    if (menu->anim_num <= 0)
    {
        menu->anim_num = 1;
        menu->anim_no = 0;
        menu->frame = 0;
        menu->frame_num = 1;
        return;
    }

    if (menu->anim_no >= menu->anim_num)
    {
        menu->anim_no = menu->anim_num - 1;
    }

    motSetAnime(&mot_test_wrk.ani, anm_tbl[entry->anm_no], menu->anim_no);
    motInitInterpAnime(&mot_test_wrk.ani, 1);
    mot_test_wrk.ani.mot.reso = 1;

    hs = (HeaderSection *)mot_test_wrk.ani.base_p;
    motSetHierarchy(GetCoordP(hs), mot_test_wrk.ani.mot.dat);

    menu->frame = 0;
    menu->frame_num = MotTestFrameCount(mot_test_wrk.ani.mot.dat);
    mot_test_wrk.ani.mot.all_cnt = menu->frame_num;
}

static void MotTestSetupTexture(void)
{
    const MOT_MENU_ENTRY *entry;

    entry = MotMenuEntry(mot_test_wrk.menu.entry_no);

    if (entry->mdl_no == M000_MIKU || entry->mdl_no == M001_MAFUYU)
    {
        if (pmanpk2[entry->mdl_no] != NULL)
        {
            SetManmdlTm2(pmanpk2[entry->mdl_no], 0, 1);
        }
    }
    else if (mot_test_wrk.ani.mdl_p != NULL)
    {
        SetEneVram(mot_test_wrk.ani.mdl_p, 0x2d00);
    }
}

static void MotTestRequestLoad(void)
{
    mot_test_wrk.menu.loaded = 0;
    mot_test_wrk.load_step = MOT_TEST_LOAD_MODEL;
    mot_test_wrk.load_id = -1;
    mot_test_wrk.menu.anim_no = 0;
    mot_test_wrk.menu.frame = 0;
    mot_test_wrk.menu.frame_num = 1;
}

static void MotTestLoadCtrl(void)
{
    const MOT_MENU_ENTRY *entry;
    u_int *mdl_p;
    u_int *anm_p;
    u_int *pkt_p;

    entry = MotMenuEntry(mot_test_wrk.menu.entry_no);

    switch (mot_test_wrk.load_step)
    {
    case MOT_TEST_LOAD_MODEL:
        motInitMsn();
        InitEneVramCtrl();
        mot_test_wrk.load_id = LoadReq(M000_MIKU_MDL + entry->mdl_no, MODEL_ADDRESS);
        mot_test_wrk.load_step = MOT_TEST_WAIT_MODEL;
        break;
    case MOT_TEST_WAIT_MODEL:
        if (IsLoadEnd(mot_test_wrk.load_id) == 0)
        {
            break;
        }

        motInitEnemyMdl((u_int *)MikuPan_GetHostAddress(MODEL_ADDRESS), entry->mdl_no);
        mot_test_wrk.load_step = MOT_TEST_LOAD_ANIM;
        break;
    case MOT_TEST_LOAD_ANIM:
        mot_test_wrk.load_id = LoadReq(M000_MIKU_ANM + entry->anm_no, ANIM_ADDRESS);
        mot_test_wrk.load_step = MOT_TEST_WAIT_ANIM;
        break;
    case MOT_TEST_WAIT_ANIM:
        if (IsLoadEnd(mot_test_wrk.load_id) == 0)
        {
            break;
        }

        mdl_p = MotTestAniModelPointer(entry->mdl_no);
        anm_p = (u_int *)MikuPan_GetHostAddress(ANIM_ADDRESS);
        pkt_p = GetPakTaleAddr(anm_p);

        motInitAniCtrl(&mot_test_wrk.ani, anm_p, mdl_p, pkt_p, entry->mdl_no, entry->anm_no);
        MotTestSetAnim();
        MotTestSetupTexture();

        mot_test_wrk.menu.loaded = 1;
        mot_test_wrk.load_step = MOT_TEST_READY;
        break;
    case MOT_TEST_READY:
        break;
    }
}

static void MotTestAdvanceFrame(void)
{
    MOT_MENU_WRK *menu;

    menu = &mot_test_wrk.menu;

    if (menu->play == 0 || menu->frame_num <= 1)
    {
        return;
    }

    menu->frame++;

    if (menu->frame >= menu->frame_num)
    {
        if (menu->loop != 0)
        {
            menu->frame = 0;
        }
        else
        {
            menu->frame = menu->frame_num - 1;
            menu->play = 0;
        }
    }
}

static void MotTestDrawModel(void)
{
    HeaderSection *hs;
    SgCOORDUNIT *cp;
    const MOT_MENU_ENTRY *entry;
    float scale;

    if (mot_test_wrk.menu.loaded == 0 || mot_test_wrk.ani.base_p == NULL)
    {
        return;
    }

    entry = MotMenuEntry(mot_test_wrk.menu.entry_no);
    hs = (HeaderSection *)mot_test_wrk.ani.base_p;

    motSetCoordFrame(&mot_test_wrk.ani, mot_test_wrk.menu.frame);

    if (mot_test_wrk.ani.mim_num != 0 || mot_test_wrk.ani.wmim_num != 0 || mot_test_wrk.ani.bg_num != 0)
    {
        SceneMimSetVertex(&mot_test_wrk.ani, mot_test_wrk.menu.frame);
    }

    cp = GetCoordP(hs);
    sceVu0UnitMatrix(cp->matrix);

    scale = manmdl_dat[entry->mdl_no].scale;

    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }

    scale = 25.0f / scale;
    cp->matrix[0][0] = scale;
    cp->matrix[1][1] = scale;
    cp->matrix[2][2] = scale;
    cp->matrix[3][3] = 1.0f;

    sceVu0RotMatrixX(cp->matrix, cp->matrix, PI);
    sceVu0RotMatrixY(cp->matrix, cp->matrix, mot_test_wrk.menu.rot_y);

    cp->matrix[3][0] = 0.0f;
    cp->matrix[3][1] = 0.0f;
    cp->matrix[3][2] = 0.0f;
    cp->matrix[3][3] = 1.0f;

    CalcCoordinate(cp, hs->blocks - 1);

    if (mot_test_wrk.ani.cloth_ctrl != NULL)
    {
        acsClothCtrl(&mot_test_wrk.ani, mot_test_wrk.ani.mpk_p, entry->mdl_no, 0);
    }

    ManmdlSetAlpha(hs, 0x7f);
    ManTexflush();
    SgSortUnitKind(hs, -1);

    if (entry->mdl_no == M000_MIKU || entry->mdl_no == M001_MAFUYU)
    {
        DrawGirlSubObj(mot_test_wrk.ani.mpk_p, 0x7f);
    }
    else
    {
        DrawEneSubObj(mot_test_wrk.ani.mpk_p, 0x7f, 0x7f);
    }

    MotTestAdvanceFrame();
}

static void MotTestDraw3D(void)
{
    objInit();
    MotTestSetCamera();
    ClearTextureCache();
    SgTEXTransEnable();
    SetEnvironment();
    SgSetFog(0.0f, 255.0f, 1000.0f, 10000.0f, 0, 0, 0);
    MotTestSetLight();
    MotTestDrawModel();
    CheckDMATrans();
}

static void MotTestUpdateMenuState(void)
{
    switch (mot_test_wrk.load_step)
    {
    case MOT_TEST_READY:
        mot_test_wrk.menu.state_str = "READY";
        break;
    case MOT_TEST_WAIT_MODEL:
        mot_test_wrk.menu.state_str = "LOAD MODEL";
        break;
    case MOT_TEST_WAIT_ANIM:
        mot_test_wrk.menu.state_str = "LOAD ANIM";
        break;
    default:
        mot_test_wrk.menu.state_str = "LOAD REQ";
        break;
    }
}

void MotTestInit(void)
{
    memset(&mot_test_wrk, 0, sizeof(mot_test_wrk));

    gra2dInitST();
    Init3D();
    InitialDmaBuffer();
    InitEneVramCtrl();
    motInitMsn();

    MotMenuInit(&mot_test_wrk.menu);

    MotTestRequestLoad();
}

void MotTestCtrl(void)
{
    int action;

    MotTestLoadCtrl();

    action = MotMenuCtrl(&mot_test_wrk.menu);

    if (action & MOT_MENU_ACT_EXIT)
    {
        OutGameModeChange(OUTGAME_MODE_TITLE);
        return;
    }

    if (action & MOT_MENU_ACT_ENTRY_CHANGED)
    {
        MotTestRequestLoad();
    }

    if (action & MOT_MENU_ACT_ANIM_CHANGED)
    {
        MotTestSetAnim();
    }

    if (action & MOT_MENU_ACT_RELOAD)
    {
        MotTestRequestLoad();
    }

    MotTestDraw3D();
    MotTestUpdateMenuState();
    MotMenuDraw(&mot_test_wrk.menu);
    gra2dDraw(GRA2D_CALL_OG);
}
