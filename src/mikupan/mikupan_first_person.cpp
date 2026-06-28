#include "common.h"
#include "enums.h"

#include "main/glob.h"
#include "common/ul_math.h"
#include "ingame/plyr/plyr_ctl.h"
#include "ingame/plyr/unit_ctl.h"
#include "graphics/graph3d/sglib.h"
#include "mikupan/mikupan_controller.h"
#include "mikupan/mikupan_first_person.h"
#include "os/key_cnf.h"
#include "sce/libvu0.h"

#include <math.h>

#define PI 3.1415928f
#define DEG2RAD(x) ((float)(x) * PI / 180.0f)
#define FINDER_MOUSE_RAD_PER_PIXEL 0.004f

int mikupan_first_person_enabled = 0;
int mikupan_first_person_r3_toggle_enabled = 0;
int mikupan_first_person_auto_run_enabled = 1;
float mikupan_first_person_eye_height = 687.0f;
float mikupan_first_person_eye_forward = -100.0f;
float mikupan_first_person_look_distance = 1000.0f;
float mikupan_first_person_fov_deg = 70.0f;
float mikupan_first_person_stick_yaw_speed_deg = 2.4f;
float mikupan_first_person_stick_pitch_speed_deg = 2.0f;

static int mikupan_first_person_active = 0;

static int MikuPan_FirstPersonMotionDrivenMoveActive(void)
{
    return (plyr_wrk.mvsta & 0x200000) != 0;
}

static int MikuPan_FirstPersonSpecialMoveActive(void)
{
    return (plyr_wrk.mvsta & 0xfff0) != 0;
}

static void MikuPan_FirstPersonClearWalkRunState(void)
{
    plyr_wrk.mvsta &= ~(0x80000 | 0x8 | 0x4 | 0x2 | 0x1);
}

static void MikuPan_FirstPersonApplyAutoRunSpecialMove(void)
{
    if (!mikupan_first_person_auto_run_enabled)
    {
        return;
    }

    if (plyr_wrk.mvsta & 0x10)
    {
        plyr_wrk.mvsta &= ~0x10;
        plyr_wrk.mvsta |= 0x100;
    }

    if (plyr_wrk.mvsta & 0x20)
    {
        plyr_wrk.mvsta &= ~0x20;
        plyr_wrk.mvsta |= 0x200;
    }

    if (plyr_wrk.mvsta & 0x40)
    {
        plyr_wrk.mvsta &= ~0x40;
        plyr_wrk.mvsta |= 0x400;
    }

    if (plyr_wrk.mvsta & 0x80)
    {
        plyr_wrk.mvsta &= ~0x80;
        plyr_wrk.mvsta |= 0x800;
    }
}

static int MikuPan_FirstPersonCanOwnGameplay(void)
{
    if (!mikupan_first_person_enabled)
    {
        return 0;
    }

    if (ingame_wrk.mode != INGAME_MODE_NOMAL)
    {
        return 0;
    }

    if (plyr_wrk.mode != PMODE_NORMAL)
    {
        return 0;
    }

    if (plyr_wrk.cond == 1 || plyr_wrk.cond == 2)
    {
        return 0;
    }

    return 1;
}

static int MikuPan_FirstPersonCanOwnCamera(void)
{
    if (!mikupan_first_person_enabled)
    {
        return 0;
    }

    if (ingame_wrk.mode != INGAME_MODE_NOMAL
        && ingame_wrk.mode != INGAME_MODE_GET_ITEM)
    {
        return 0;
    }

    if (plyr_wrk.cond == 1 || plyr_wrk.cond == 2)
    {
        return 0;
    }

    switch (plyr_wrk.mode)
    {
        case PMODE_NORMAL:
        case PMODE_FINDER_IN:
        case PMODE_FINDER_END:
        case PMODE_MSG_DISP:
        case PMODE_ANIME:
            return 1;
        default:
            return 0;
    }
}

static float MikuPan_FirstPersonClampFloat(float value, float min, float max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

static float MikuPan_FirstPersonApplyDeadzone(float value, float deadzone)
{
    float sign;
    float avalue = fabsf(value);

    if (avalue <= deadzone)
    {
        return 0.0f;
    }

    sign = value < 0.0f ? -1.0f : 1.0f;
    value = (avalue - deadzone) / (127.0f - deadzone);

    return MikuPan_FirstPersonClampFloat(value * sign, -1.0f, 1.0f);
}

static float MikuPan_FirstPersonPadAxis(u_char value)
{
    return MikuPan_FirstPersonApplyDeadzone((float)value - 128.0f, 24.0f);
}

static void MikuPan_FirstPersonClampPitch(void)
{
    if (plyr_wrk.frot_x > DEG2RAD(60.0f))
    {
        plyr_wrk.frot_x = DEG2RAD(60.0f);
    }
    else if (plyr_wrk.frot_x < DEG2RAD(-60.0f))
    {
        plyr_wrk.frot_x = DEG2RAD(-60.0f);
    }

    RotLimitChk(&plyr_wrk.frot_x);
}

static void MikuPan_FirstPersonUpdateLook(void)
{
    MOVE_BOX* mb = &plyr_wrk.move_box;
    float yaw = 0.0f;
    float pitch = 0.0f;

    if (pad[0].id == 0x79)
    {
        yaw = MikuPan_FirstPersonPadAxis(pad[0].analog[0]);
        pitch = MikuPan_FirstPersonPadAxis(pad[0].analog[1]);

        if (yaw != 0.0f)
        {
            mb->rot[1] += yaw * DEG2RAD(mikupan_first_person_stick_yaw_speed_deg);
            RotLimitChk(&mb->rot[1]);
        }

        if (pitch != 0.0f)
        {
            /* Stick up is a value below centre.  Make that look up by default. */
            pitch = -pitch;

            if (MikuPan_KeyProfileUsesFinderReverseY())
            {
                pitch = -pitch;
            }

            plyr_wrk.frot_x +=
                pitch * DEG2RAD(mikupan_first_person_stick_pitch_speed_deg);
            MikuPan_FirstPersonClampPitch();
        }
    }

    MikuPan_FinderMouseRequest();
    {
        float mdx = 0.0f;
        float mdy = 0.0f;
        float sens;
        float fov_scale;

        if (MikuPan_FinderMouseConsume(&mdx, &mdy))
        {
            sens = MikuPan_FinderMouseSensitivity() * FINDER_MOUSE_RAD_PER_PIXEL;

            fov_scale = camera.fov / DEG2RAD(51.0f);
            fov_scale = MikuPan_FirstPersonClampFloat(fov_scale, 0.25f, 1.0f);
            sens *= fov_scale;

            mb->rot[1] += mdx * sens;
            RotLimitChk(&mb->rot[1]);

            pitch = -mdy * sens;
            if (MikuPan_KeyProfileUsesFinderReverseY())
            {
                pitch = -pitch;
            }

            plyr_wrk.frot_x += pitch;
            MikuPan_FirstPersonClampPitch();
        }
    }
}

static void MikuPan_FirstPersonBuildCamera(void)
{
    MOVE_BOX* mb = &plyr_wrk.move_box;
    sceVu0FVECTOR eye_offset = {
        0.0f,
        -mikupan_first_person_eye_height,
        mikupan_first_person_eye_forward,
        0.0f
    };
    sceVu0FVECTOR look_offset = {
        0.0f,
        0.0f,
        mikupan_first_person_look_distance,
        0.0f
    };
    sceVu0FVECTOR yaw_rot = {
        0.0f,
        mb->rot[1],
        0.0f,
        0.0f
    };
    sceVu0FVECTOR look_rot = {
        plyr_wrk.frot_x,
        mb->rot[1],
        0.0f,
        0.0f
    };

    RotFvector(yaw_rot, eye_offset);
    sceVu0AddVector(camera.p, mb->pos, eye_offset);

    RotFvector(look_rot, look_offset);
    sceVu0AddVector(camera.i, camera.p, look_offset);

    camera.p[3] = 1.0f;
    camera.i[3] = 1.0f;
    camera.roll = PI;
    camera.fov = DEG2RAD(mikupan_first_person_fov_deg);

    plyr_wrk.prot = mb->rot[1];
}

static int MikuPan_FirstPersonReadMoveAxes(float* out_right, float* out_forward)
{
    float right = 0.0f;
    float forward = 0.0f;
    float len;

    if (DPAD_UP_PRESSED() != 0)
    {
        forward += 1.0f;
    }
    if (DPAD_DOWN_PRESSED() != 0)
    {
        forward -= 1.0f;
    }
    if (DPAD_RIGHT_PRESSED() != 0)
    {
        right += 1.0f;
    }
    if (DPAD_LEFT_PRESSED() != 0)
    {
        right -= 1.0f;
    }

    if (right == 0.0f && forward == 0.0f && pad[0].id == 0x79)
    {
        right = MikuPan_FirstPersonPadAxis(pad[0].analog[2]);
        forward = -MikuPan_FirstPersonPadAxis(pad[0].analog[3]);
    }

    len = sqrtf(right * right + forward * forward);
    if (len <= 0.0f)
    {
        *out_right = 0.0f;
        *out_forward = 0.0f;
        return 0;
    }

    if (len > 1.0f)
    {
        right /= len;
        forward /= len;
    }

    *out_right = right;
    *out_forward = forward;
    return 1;
}

int MikuPan_FirstPersonEnabled(void)
{
    return mikupan_first_person_enabled;
}

int MikuPan_FirstPersonActive(void)
{
    return mikupan_first_person_active;
}

void MikuPan_FirstPersonSetEnabled(int enabled)
{
    enabled = enabled ? 1 : 0;

    if (enabled && !mikupan_first_person_enabled)
    {
        plyr_wrk.fp[0] = 0x140;
        plyr_wrk.fp[1] = 0xd1;
        plyr_wrk.frot_x = 0.0f;
    }

    mikupan_first_person_enabled = enabled;

    if (!mikupan_first_person_enabled)
    {
        mikupan_first_person_active = 0;
    }
}

void MikuPan_FirstPersonToggle(void)
{
    MikuPan_FirstPersonSetEnabled(!mikupan_first_person_enabled);
}

void MikuPan_FirstPersonBeginFrame(void)
{
    mikupan_first_person_active = 0;
}

void MikuPan_FirstPersonUpdateToggle(void)
{
    if (!mikupan_first_person_r3_toggle_enabled)
    {
        return;
    }

    if (R3_PRESSED() != 1)
    {
        return;
    }

    if (ingame_wrk.mode != INGAME_MODE_NOMAL)
    {
        return;
    }

    if (plyr_wrk.mode != PMODE_NORMAL && !mikupan_first_person_enabled)
    {
        return;
    }

    MikuPan_FirstPersonToggle();
}

int MikuPan_FirstPersonCameraCtrl(void)
{
    int can_own_gameplay;

    if (!MikuPan_FirstPersonCanOwnCamera())
    {
        mikupan_first_person_active = 0;
        return 0;
    }

    mikupan_first_person_active = 1;
    can_own_gameplay = MikuPan_FirstPersonCanOwnGameplay();

    if (can_own_gameplay && !MikuPan_FirstPersonMotionDrivenMoveActive())
    {
        MikuPan_FirstPersonUpdateLook();
    }

    MikuPan_FirstPersonBuildCamera();

    return 1;
}

int MikuPan_FirstPersonMoveCtrl(MOVE_BOX* mb, sceVu0FVECTOR tv)
{
    float right;
    float forward;
    float rot;
    float speed;
    int moving;

    tv[0] = 0.0f;
    tv[1] = 0.0f;
    tv[2] = 0.0f;
    tv[3] = 0.0f;

    if (!MikuPan_FirstPersonCanOwnGameplay())
    {
        return 0;
    }

    if (MikuPan_FirstPersonMotionDrivenMoveActive())
    {
        MikuPan_FirstPersonApplyAutoRunSpecialMove();
        GetMoveSpeed(tv);
        RotFvector(mb->rot, tv);
        return 1;
    }

    if (MikuPan_FirstPersonSpecialMoveActive())
    {
        MikuPan_FirstPersonApplyAutoRunSpecialMove();
        GetMoveSpeed(tv);
        RotFvector(mb->rot, tv);
        return 1;
    }

    moving = MikuPan_FirstPersonReadMoveAxes(&right, &forward);
    rot = moving ? SgAtan2f(right, forward) : 10.0f;

    mb->rspd[1] = 0.0f;
    mb->loop = 0;

    if (!moving)
    {
        MikuPan_FirstPersonClearWalkRunState();
        return 1;
    }

    PlyrMoveStaChk(rot);

    if (mikupan_first_person_auto_run_enabled && plyr_wrk.dwalk_tm == 0)
    {
        plyr_wrk.mvsta &= ~(0x8 | 0x4 | 0x2 | 0x1);
        plyr_wrk.mvsta |= 0x1;
    }

    if (!moving)
    {
        return 1;
    }

    speed = GetPlyrSpd();
    if (speed == 0.0f)
    {
        if (plyr_wrk.move_mot != 0 || MikuPan_FirstPersonSpecialMoveActive())
        {
            return 1;
        }

        speed = mikupan_first_person_auto_run_enabled ? 11.0f : 5.5f;
    }

#ifdef BUILD_EU_VERSION
    if ((plyr_wrk.mvsta & 0xfff0) == 0)
    {
        speed *= sys_wrk.move_conv;
    }
#endif

    tv[0] = right * speed;
    tv[2] = forward * speed;
    RotFvector(mb->rot, tv);

    return 1;
}

int MikuPan_FirstPersonShouldBlockSpotMove(void)
{
    return MikuPan_FirstPersonCanOwnGameplay();
}

int MikuPan_FirstPersonShouldHidePlayerModel(void)
{
    if (mikupan_first_person_active)
    {
        return 1;
    }

    if (!mikupan_first_person_enabled)
    {
        return 0;
    }

    if (MikuPan_FirstPersonCanOwnGameplay())
    {
        return 1;
    }

    if (plyr_wrk.mode == PMODE_FINDER_IN || plyr_wrk.mode == PMODE_FINDER)
    {
        return 1;
    }

    return 0;
}

int MikuPan_FirstPersonUseFinderFog(void)
{
    return mikupan_first_person_active;
}

int MikuPan_FirstPersonShouldUseCameraSpotlight(void)
{
    return mikupan_first_person_active;
}