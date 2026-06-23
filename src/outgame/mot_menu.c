#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "outgame/mot_menu.h"

#include "main/glob.h"

#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/message.h"

#include <stdio.h>

#define PI 3.1415927f
#define DEG2RAD(x) ((float)(x) * PI / 180.0f)

#define MOT_MENU_ROW_NUM 11
#define MOT_MENU_ENTRY_NUM ((int)(sizeof(mot_menu_entries) / sizeof(mot_menu_entries[0])))

static const MOT_MENU_ENTRY mot_menu_entries[] = {
    {M000_MIKU,      A000_MIKU,      "M000 MIKU"},
    {M001_MAFUYU,    A001_MAFUYU,    "M001 MAFUYU"},
    {M010_HENREI,    A010_HENREI,    "M010 HENREI"},
    {M011_JYOREI,    A011_JYOREI,    "M011 JYOREI"},
    {M012_SAKKAREI,  A012_SAKKAREI,  "M012 SAKKAREI"},
    {M013_TENAGA,    A013_TENAGA,    "M013 TENAGA"},
    {M014_KAMIONNA,  A014_KAMIONNA,  "M014 KAMIONNA"},
    {M015_KOMUSO,    A015_KOMUSO,    "M015 KOMUSO"},
    {M016_MINREI,    A016_MINREI,    "M016 MINREI"},
    {M018_MAYOIGO1,  A018_MAYOIGO1,  "M018 MAYOIGO1"},
    {M019_BOUREI1,   A019_BOUREI1,   "M019 BOUREI1"},
    {M020_SAKASA,    A020_SAKASA,    "M020 SAKASA"},
    {M021_KUBI,      A021_KUBI,      "M021 KUBI"},
    {M022_MAYOIGO2,  A022_MAYOIGO2,  "M022 MAYOIGO2"},
    {M023_MAYOIGO3,  A023_MAYOIGO3,  "M023 MAYOIGO3"},
    {M024_MEKAKUSHI, A024_MEKAKUSHI, "M024 MEKAKUSHI"},
    {M025_RTE,       A025_RTE,       "M025 RTE"},
    {M027_KONNA,     A027_KONNA,     "M027 KONNA"},
    {M028_KOTOKO,    A028_KOTOKO,    "M028 KOTOKO"},
    {M031_HAIREI,    A031_HAIREI,    "M031 HAIREI"},
    {M032_SHINKAN1,  A032_SHINKAN1,  "M032 SHINKAN1"},
    {M033_MINTSUMA,  A033_MINTSUMA,  "M033 MINTSUMA"},
    {M034_SHINKAN2,  A034_SHINKAN2,  "M034 SHINKAN2"},
    {M035_SHINKAN3,  A035_SHINKAN3,  "M035 SHINKAN3"},
    {M036_SHINKAN4,  A036_SHINKAN4,  "M036 SHINKAN4"},
    {M037_TOUSHU,    A037_TOUSHU,    "M037 TOUSHU"},
    {M038_NAWAMIKO,  A038_NAWAMIKO,  "M038 NAWAMIKO"},
    {M039_MAGONLY,   A039_DUMMY,     "M039 MAGONLY"},
    {M040_MAGATOKI,  A040_MAGATOKI,  "M040 MAGATOKI"},
    {M041_DUMMY,     A041_DUMMY,     "M041 DUMMY"},
    {M042_SYOUKI2,   A042_SYOUKIA,   "M042 SYOUKI2"},
    {M043_SYOUKI3,   A043_SYOUKIB,   "M043 SYOUKI3"},
    {M044_SYOUKI4,   A044_SYOUKIC,   "M044 SYOUKI4"},
    {M051_MKIRIE,    A051_MKIRIE,    "M051 MKIRIE"},
    {M058_BOUREI2,   A058_BOUREI2,   "M058 BOUREI2"},
    {M800_BOUREI1,   A800_BOUREI1,   "M800 BOUREI1"},
    {M801_SAKASA,    A801_SAKASA,    "M801 SAKASA"},
    {M802_KUBI,      A802_KUBI,      "M802 KUBI"},
    {M803_KONNA,     A803_KONNA,     "M803 KONNA"},
    {M804_KOTOKO,    A804_KOTOKO,    "M804 KOTOKO"},
    {M005_SAKKA,     A001_SAKKA,     "F001 SAKKA"},
    {M003_HENSYU,    A100_HENSYU,    "F100 HENSYU"},
    {M003_HENSYU,    A101_HENSYU,    "F101 HENSYU"},
    {M003_HENSYU,    A102_HENSYU,    "F102 HENSYU"},
    {M010_HENREI,    A103_HENREI,    "F103 HENREI"},
    {M010_HENREI,    A104_HENREI,    "F104 HENREI"},
    {M003_HENSYU,    A105_HENSYU,    "F105 HENSYU"},
    {M004_JYOSYU,    A106_JYOSYU,    "F106 JYOSYU"},
    {M024_MEKAKUSHI, A107_MEKAKUSHI, "F107 MEKAKUSHI"},
    {M004_JYOSYU,    A108_JYOSYU,    "F108 JYOSYU"},
    {M004_JYOSYU,    A109_JYOSYU,    "F109 JYOSYU"},
    {M033_MINTSUMA,  A110_MINTSUMA,  "F110 MINTSUMA"},
    {M021_KUBI,      A111_KUBI,      "F111 KUBI"},
    {M005_SAKKA,     A112_SAKKA,     "F112 SAKKA"},
    {M006_KOKIRIE,   A113_KOKIRIE,   "F113 KOKIRIE"},
    {M005_SAKKA,     A114_SAKKA,     "F114 SAKKA"},
    {M047_SAKKAN,    A115_SAKKAN,    "F115 SAKKAN"},
    {M001_MAFUYU,    A116_MAFUYU,    "F116 MAFUYU"},
    {M004_JYOSYU,    A119_JYOSYU,    "F119 JYOSYU"},
    {M006_KOKIRIE,   A120_KOKIRIE,   "F120 KOKIRIE"},
    {M004_JYOSYU,    A122_JYOSYU,    "F122 JYOSYU"},
    {M005_SAKKA,     A125_SAKKA,     "F125 SAKKA"},
    {M020_SAKASA,    A126_SAKASA,    "F126 SAKASA"},
    {M003_HENSYU,    A128_HENSYU,    "F128 HENSYU"},
    {M010_HENREI,    A129_HENREI,    "F129 HENREI"},
    {M009_HENSHITAI, A130_HENSHITAI, "F130 HENSHITAI"},
    {M029_MAYOIGO4,  A200_MAYOIGO4,  "F200 MAYOIGO4"},
    {M001_MAFUYU,    A205_MAFUYU,    "F205 MAFUYU"},
    {M018_MAYOIGO1,  A206_MAYOIGO1,  "F206 MAYOIGO1"},
    {M026_MINZOKU,   A207_MINZOKU,   "F207 MINZOKU"},
    {M006_KOKIRIE,   A209_KOKIRIE,   "F209 KOKIRIE"},
    {M026_MINZOKU,   A211_MINZOKU,   "F211 MINZOKU"},
    {M026_MINZOKU,   A213_MINZOKU,   "F213 MINZOKU"},
    {M026_MINZOKU,   A216_MINZOKU,   "F216 MINZOKU"},
    {M059_JITOUSHU,  A300_JITOUSHU,  "F300 JITOUSHU"},
    {M059_JITOUSHU,  A301_JITOUSHU,  "F301 JITOUSHU"},
    {M006_KOKIRIE,   A400_KOKIRIE,   "F400 KOKIRIE"},
};

static void MotMenuSetSquare(int pri, float x, float y, float w, float h, u_char r, u_char g, u_char b, u_char a)
{
    SetSquare(pri, x - 320.0f, y - 224.0f, (x - 320.0f) + w, y - 224.0f,
              x - 320.0f, (y - 224.0f) + h, (x - 320.0f) + w, (y - 224.0f) + h,
              r, g, b, a);
}

static void MotMenuClampFloat(float *value, float min, float max)
{
    if (*value < min)
    {
        *value = min;
    }
    else if (*value > max)
    {
        *value = max;
    }
}

static void MotMenuWrapInt(int *value, int min, int max)
{
    if (*value < min)
    {
        *value = max;
    }
    else if (*value > max)
    {
        *value = min;
    }
}

static void MotMenuDrawLine(int line, char *str)
{
    SetASCIIString2(1, 40.0f, line * 14 + 48, 1, 0x80, 0x80, 0x80, str);
}

int MotMenuEntryNum(void)
{
    return MOT_MENU_ENTRY_NUM;
}

const MOT_MENU_ENTRY *MotMenuEntry(int entry_no)
{
    if (entry_no < 0)
    {
        entry_no = 0;
    }
    else if (entry_no >= MOT_MENU_ENTRY_NUM)
    {
        entry_no = MOT_MENU_ENTRY_NUM - 1;
    }

    return &mot_menu_entries[entry_no];
}

void MotMenuInit(MOT_MENU_WRK *menu)
{
    menu->entry_no = 0;
    menu->anim_no = 0;
    menu->anim_num = 1;
    menu->frame = 0;
    menu->frame_num = 1;
    menu->menu_csr = 0;
    menu->play = 1;
    menu->loop = 1;
    menu->loaded = 0;
    menu->rot_y = 0.0f;
    menu->height = -450.0f;
    menu->zoom = -1600.0f;
    menu->light_power = 0.95f;
    menu->state_str = "LOAD REQ";
}

int MotMenuCtrl(MOT_MENU_WRK *menu)
{
    int changed;
    int action;

    action = MOT_MENU_ACT_NONE;

    if (pad[0].one & 0x40)
    {
        return MOT_MENU_ACT_EXIT;
    }

    if (pad[0].one & 0x800)
    {
        menu->play ^= 1;
    }

    if (pad[0].rpt & 0x4000)
    {
        menu->menu_csr++;
        MotMenuWrapInt(&menu->menu_csr, 0, MOT_MENU_ROW_NUM - 1);
    }
    else if (pad[0].rpt & 0x1000)
    {
        menu->menu_csr--;
        MotMenuWrapInt(&menu->menu_csr, 0, MOT_MENU_ROW_NUM - 1);
    }

    changed = 0;

    if (pad[0].rpt & 0x2000)
    {
        changed = 1;
    }
    else if (pad[0].rpt & 0x8000)
    {
        changed = -1;
    }

    if (changed != 0)
    {
        switch (menu->menu_csr)
        {
        case 0:
            menu->entry_no += changed;
            MotMenuWrapInt(&menu->entry_no, 0, MOT_MENU_ENTRY_NUM - 1);
            action |= MOT_MENU_ACT_ENTRY_CHANGED;
            break;
        case 1:
            if (menu->loaded != 0)
            {
                menu->anim_no += changed;
                MotMenuWrapInt(&menu->anim_no, 0, menu->anim_num - 1);
                action |= MOT_MENU_ACT_ANIM_CHANGED;
            }
            break;
        case 2:
            menu->play = 0;
            menu->frame += changed;
            MotMenuWrapInt(&menu->frame, 0, menu->frame_num - 1);
            break;
        case 5:
            menu->rot_y += DEG2RAD(5.0f * changed);
            if (menu->rot_y > PI)
            {
                menu->rot_y -= PI * 2.0f;
            }
            else if (menu->rot_y < -PI)
            {
                menu->rot_y += PI * 2.0f;
            }
            break;
        case 6:
            menu->height += 25.0f * changed;
            MotMenuClampFloat(&menu->height, -1400.0f, 600.0f);
            break;
        case 7:
            menu->zoom += 50.0f * changed;
            MotMenuClampFloat(&menu->zoom, -3000.0f, -200.0f);
            break;
        case 8:
            menu->light_power += 0.05f * changed;
            MotMenuClampFloat(&menu->light_power, 0.05f, 2.0f);
            break;
        default:
            break;
        }
    }

    if (pad[0].one & 0x20)
    {
        switch (menu->menu_csr)
        {
        case 3:
            menu->play ^= 1;
            break;
        case 4:
            menu->loop ^= 1;
            break;
        case 9:
            action |= MOT_MENU_ACT_RELOAD;
            break;
        case 10:
            action |= MOT_MENU_ACT_EXIT;
            break;
        default:
            break;
        }
    }

    return action;
}

void MotMenuDraw(MOT_MENU_WRK *menu)
{
    char tmp_str[256];
    const MOT_MENU_ENTRY *entry;

    entry = MotMenuEntry(menu->entry_no);

    SetASCIIString2(1, 40.0f, 28.0f, 0, 0x20, 0x80, 0x80, "MOTION TEST");

    sprintf(tmp_str, "PACK  %02d/%02d  MDL %02d ANM %02d  %s",
            menu->entry_no, MOT_MENU_ENTRY_NUM - 1, entry->mdl_no, entry->anm_no, entry->name);
    MotMenuDrawLine(0, tmp_str);

    sprintf(tmp_str, "ANIM  %02d/%02d", menu->anim_no, menu->anim_num - 1);
    MotMenuDrawLine(1, tmp_str);

    sprintf(tmp_str, "FRAME %04d/%04d", menu->frame, menu->frame_num - 1);
    MotMenuDrawLine(2, tmp_str);

    sprintf(tmp_str, "PLAY  %s", menu->play != 0 ? "ON" : "OFF");
    MotMenuDrawLine(3, tmp_str);

    sprintf(tmp_str, "LOOP  %s", menu->loop != 0 ? "ON" : "OFF");
    MotMenuDrawLine(4, tmp_str);

    sprintf(tmp_str, "ROT Y %+0.2f", menu->rot_y);
    MotMenuDrawLine(5, tmp_str);

    sprintf(tmp_str, "HEIGHT %+0.1f", menu->height);
    MotMenuDrawLine(6, tmp_str);

    sprintf(tmp_str, "ZOOM  %+0.1f", menu->zoom);
    MotMenuDrawLine(7, tmp_str);

    sprintf(tmp_str, "LIGHT %0.2f", menu->light_power);
    MotMenuDrawLine(8, tmp_str);

    sprintf(tmp_str, "RELOAD");
    MotMenuDrawLine(9, tmp_str);

    sprintf(tmp_str, "EXIT");
    MotMenuDrawLine(10, tmp_str);

    sprintf(tmp_str, "STATE %s   X DECIDE  O EXIT  START PLAY", menu->state_str);
    SetASCIIString2(1, 40.0f, 410.0f, 1, 0x80, 0x80, 0x80, tmp_str);

    MotMenuSetSquare(2, 36.0f, menu->menu_csr * 14 + 46, 420.0f, 12.0f, 0x50, 0x50, 0x64, 0x50);
}
