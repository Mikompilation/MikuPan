#include "typedefs.h"
#include "mikupan_ui_cheats.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "ingame/camera/camera.h"
#include "ingame/menu/item.h"
#include "main/glob.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/mikupan_utils.h"
#include "outgame/mode_slct.h"

static int cheat_photo_mode = 0;
static int cheat_photo_mode_previous_cam_mode = 0;
static int cheat_tofu_mode = 0;
static float cheat_tofu_color[3] = {0.86f, 0.81f, 0.63f};

void MikuPan_CheatSetInventoryItem(int item_no, int value)
{
    if (item_no < 0 || item_no >= 200)
    {
        return;
    }

    poss_item[item_no] =
        (u_char) MikuPan_ClampInt(value, 0, item_no == 9 ? 1 : 99);
}

const char* MikuPan_CheatInventoryItemName(int item_no)
{
    switch (item_no)
    {
    case 1:
        return "Type-14 Film";
    case 2:
        return "Type-37 Film";
    case 3:
        return "Type-74 Film";
    case 4:
        return "Type-90 Film";
    case 5:
        return "Spirit Stone";
    case 6:
        return "Manyogan";
    case 7:
        return "Sacred Water";
    case 8:
        return "Stone Mirror";
    case 9:
        return "Camera";
    default:
        return NULL;
    }
}

void MikuPan_UiInventoryItemSlider(int item_no)
{
    char label[64];
    int value;
    const char* name = MikuPan_CheatInventoryItemName(item_no);

    if (name != NULL)
    {
        snprintf(label, sizeof(label), "%s##cheat_item_%02d", name, item_no);
    }
    else
    {
        snprintf(label, sizeof(label), "Item %02d##cheat_item_%02d", item_no,
                 item_no);
    }

    value = poss_item[item_no];
    if (igSliderInt(label, &value, 0, item_no == 9 ? 1 : 99, "%d", 0))
    {
        MikuPan_CheatSetInventoryItem(item_no, value);
    }
}

void MikuPan_CheatSetAllMenuItems(int value)
{
    const int count = (int) (sizeof(item_sort) / sizeof(item_sort[0]));

    for (int i = 0; i < count; i++)
    {
        MikuPan_CheatSetInventoryItem(item_sort[i], value);
    }
}

void MikuPan_CheatSetPhotoMode(int enabled)
{
    if (enabled)
    {
        if (!cheat_photo_mode)
        {
            cheat_photo_mode_previous_cam_mode = dbg_wrk.cam_mode;
        }

        cheat_photo_mode = 1;
        camera_photo_mode_enabled = 1;
        dbg_wrk.cam_mode = 1;
        return;
    }

    if (cheat_photo_mode && dbg_wrk.cam_mode == 1)
    {
        dbg_wrk.cam_mode = cheat_photo_mode_previous_cam_mode;
    }

    cheat_photo_mode = 0;
    camera_photo_mode_enabled = 0;
}

void MikuPan_CheatSyncPhotoMode(void)
{
    if (!cheat_photo_mode)
    {
        camera_photo_mode_enabled = 0;
        return;
    }

    if (dbg_wrk.cam_mode != 1)
    {
        cheat_photo_mode = 0;
        camera_photo_mode_enabled = 0;
        return;
    }

    camera_photo_mode_enabled = 1;
}

void MikuPan_CheatMaxCoreInventory(void)
{
    MikuPan_CheatSetInventoryItem(1, 99);
    MikuPan_CheatSetInventoryItem(2, 99);
    MikuPan_CheatSetInventoryItem(3, 99);
    MikuPan_CheatSetInventoryItem(4, 99);
    MikuPan_CheatSetInventoryItem(5, 99);
    MikuPan_CheatSetInventoryItem(6, 99);
    MikuPan_CheatSetInventoryItem(7, 99);
    MikuPan_CheatSetInventoryItem(8, 99);
    MikuPan_CheatSetInventoryItem(9, 1);

    if (plyr_wrk.film_no < 1 || plyr_wrk.film_no > 4)
    {
        plyr_wrk.film_no = 1;
    }
}

void MikuPan_CheatUnlockAllCostumes(void)
{
    cribo.clear_info |= 0x1;

    if (ingame_wrk.clear_count < 3)
    {
        ingame_wrk.clear_count = 3;
    }
}

void MikuPan_CheatUnlockCameraMenu(void)
{
    MikuPan_CheatSetInventoryItem(9, 1);
    cribo.clear_info |= 0x4;
}

void MikuPan_CheatUnlockCameraAbilities(void)
{
    for (int i = 0; i < 5; i++)
    {
        cam_custom_wrk.func_sub[i] = 2;
        cam_custom_wrk.func_spe[i] = 2;
    }

    if (cam_custom_wrk.set_sub < 5)
    {
        cam_custom_wrk.func_sub[cam_custom_wrk.set_sub] = 3;
    }
    else
    {
        cam_custom_wrk.set_sub = 0xff;
    }

    if (cam_custom_wrk.set_spe < 5)
    {
        cam_custom_wrk.func_spe[cam_custom_wrk.set_spe] = 3;
    }
    else
    {
        cam_custom_wrk.set_spe = 0xff;
    }
}

void MikuPan_CheatMaxCameraUpgrades(void)
{
    cam_custom_wrk.charge_range = 3;
    cam_custom_wrk.charge_speed = 3;
    cam_custom_wrk.charge_max = 3;
    cam_custom_wrk.point = 9999999;
}

void MikuPan_CheatUnlockAll(void)
{
    MikuPan_CheatUnlockAllCostumes();
    MikuPan_CheatUnlockCameraMenu();
    MikuPan_CheatUnlockCameraAbilities();
    MikuPan_CheatMaxCameraUpgrades();
}

void MikuPan_UiInventoryCheatsMenu(void)
{
    static int all_menu_item_value = 1;
    int selected_film = plyr_wrk.film_no;

    if (igMenuItem_Bool("Max Core Inventory", NULL, false, true))
    {
        MikuPan_CheatMaxCoreInventory();
    }

    igSeparatorText("Film");
    if (igSliderInt("Equipped Film", &selected_film, 1, 4, "Type index %d", 0))
    {
        plyr_wrk.film_no = (u_char) selected_film;
    }

    MikuPan_UiInventoryItemSlider(1);
    MikuPan_UiInventoryItemSlider(2);
    MikuPan_UiInventoryItemSlider(3);
    MikuPan_UiInventoryItemSlider(4);

    igSeparatorText("Supplies");
    MikuPan_UiInventoryItemSlider(5);
    MikuPan_UiInventoryItemSlider(6);
    MikuPan_UiInventoryItemSlider(7);
    MikuPan_UiInventoryItemSlider(8);
    MikuPan_UiInventoryItemSlider(9);

    if (igBeginMenu("All Menu Items", 1))
    {
        const int count = (int) (sizeof(item_sort) / sizeof(item_sort[0]));

        igSliderInt("Value", &all_menu_item_value, 0, 99, "%d", 0);
        if (igButton("Apply to Listed Items", ImVec2{0.0f, 0.0f}))
        {
            MikuPan_CheatSetAllMenuItems(all_menu_item_value);
        }

        igSeparator();

        for (int i = 0; i < count; i++)
        {
            MikuPan_UiInventoryItemSlider(item_sort[i]);
        }

        igEndMenu();
    }
}

void MikuPan_UiUnlockCheatsMenu(void)
{
    bool costume_flag = (cribo.clear_info & 0x1) != 0;
    bool camera_menu_flag = (cribo.clear_info & 0x4) != 0;
    int clear_count = ingame_wrk.clear_count;
    int costume = cribo.costume;
    int points = (int) cam_custom_wrk.point;

    if (igMenuItem_Bool("All Unlocks", NULL, false, true))
    {
        MikuPan_CheatUnlockAll();
    }

    if (igMenuItem_Bool("Unlock All Costumes", NULL, false, true))
    {
        MikuPan_CheatUnlockAllCostumes();
        costume_flag = true;
        clear_count = ingame_wrk.clear_count;
    }

    if (igCheckbox("Costume Unlock Flag", &costume_flag))
    {
        if (costume_flag)
        {
            MikuPan_CheatUnlockAllCostumes();
            clear_count = ingame_wrk.clear_count;
        }
        else
        {
            cribo.clear_info &= (u_char) ~0x1;
        }
    }

    if (igSliderInt("Clear Count", &clear_count, 0, 108, "%d", 0))
    {
        ingame_wrk.clear_count = (u_char) clear_count;
    }

    if (igSliderInt("Selected Costume", &costume, 0, 3, "%d", 0))
    {
        cribo.costume = (u_char) costume;
    }

    igSeparatorText("Camera");

    if (igMenuItem_Bool("Unlock Camera Menu", NULL, false, true))
    {
        MikuPan_CheatUnlockCameraMenu();
        camera_menu_flag = true;
    }

    if (igCheckbox("Camera Menu Unlock Flag", &camera_menu_flag))
    {
        if (camera_menu_flag)
        {
            MikuPan_CheatUnlockCameraMenu();
        }
        else
        {
            cribo.clear_info &= (u_char) ~0x4;
        }
    }

    if (igMenuItem_Bool("Unlock Camera Abilities", NULL, false, true))
    {
        MikuPan_CheatUnlockCameraAbilities();
    }

    if (igMenuItem_Bool("Max Camera Upgrades", NULL, false, true))
    {
        MikuPan_CheatMaxCameraUpgrades();
        points = (int) cam_custom_wrk.point;
    }

    if (igInputInt("Camera Points", &points, 1000, 10000, 0))
    {
        cam_custom_wrk.point = (u_long) MikuPan_ClampInt(points, 0, 9999999);
    }
}

void MikuPan_UiCheatsRender(void)
{
    if (igBeginMenu("Cheats", 1))
    {
        if (igMenuItem_Bool("Take Screenshot", "F12", false, true))
        {
            MikuPan_ScreenshotRequest();
        }

        {
            int photo_mode = cheat_photo_mode;
            if (igCheckbox("Photo Mode", (bool*) &photo_mode))
            {
                MikuPan_CheatSetPhotoMode(photo_mode);
            }
        }

        igSeparator();

        if (igBeginMenu("Inventory", 1))
        {
            MikuPan_UiInventoryCheatsMenu();
            igEndMenu();
        }

        if (igBeginMenu("Unlocks", 1))
        {
            MikuPan_UiUnlockCheatsMenu();
            igEndMenu();
        }

        igSeparator();

        igCheckbox("Tofu Mode", (bool*) &cheat_tofu_mode);
        igColorEdit3("Tofu Color", cheat_tofu_color, 0);

        igEndMenu();
    }
}

int MikuPan_IsTofuModeEnabled(void)
{
    return cheat_tofu_mode;
}

const float* MikuPan_GetTofuColor(void)
{
    return cheat_tofu_color;
}
