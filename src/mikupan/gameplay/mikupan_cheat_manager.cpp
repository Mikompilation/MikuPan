#include "mikupan/gameplay/mikupan_cheat_manager.h"

#include "main/glob.h"
#include "outgame/btl_mode/btl_mode.h"

namespace
{
bool g_infinite_health = false;
bool g_infinite_film = false;
bool g_infinite_spirit_stone = false;
bool g_infinite_herbal_medicine = false;
bool g_infinite_sacred_water = false;
bool g_one_hit_kills = false;
bool g_max_camera_range = false;
bool g_max_camera_speed = false;
bool g_unlock_bonus_camera_options = false;
bool g_unlock_special_camera_options = false;
bool g_max_camera_upgrades = false;
bool g_max_campaign_score = false;
bool g_max_camera_points = false;
bool g_always_low_game_time = false;
bool g_faster_running = false;
bool g_faster_walking = false;
bool g_max_battle_score = false;
bool g_max_battle_high_score = false;
}

extern "C" {

void MikuPan_CheatManagerSetInfiniteHealth(int enabled)
{
    g_infinite_health = enabled != 0;
    if (g_infinite_health)
    {
        plyr_wrk.hp = 500;
    }
}

int MikuPan_CheatManagerInfiniteHealthEnabled(void)
{
    return g_infinite_health ? 1 : 0;
}

void MikuPan_CheatManagerUpdatePlayer(void)
{
    if (g_max_camera_range) cam_custom_wrk.charge_range = 3;
    if (g_max_camera_speed) cam_custom_wrk.charge_speed = 3;
    if (g_max_camera_upgrades)
    {
        cam_custom_wrk.charge_range = 3;
        cam_custom_wrk.charge_speed = 3;
        cam_custom_wrk.charge_max = 3;
    }
    if (g_unlock_bonus_camera_options)
    {
        for (int i = 0; i < 5; ++i) cam_custom_wrk.func_sub[i] = 2;
        if (cam_custom_wrk.set_sub < 5) cam_custom_wrk.func_sub[cam_custom_wrk.set_sub] = 3;
    }
    if (g_unlock_special_camera_options)
    {
        for (int i = 0; i < 5; ++i) cam_custom_wrk.func_spe[i] = 2;
        if (cam_custom_wrk.set_spe < 5) cam_custom_wrk.func_spe[cam_custom_wrk.set_spe] = 3;
    }
    if (g_infinite_health)
    {
        plyr_wrk.hp = 500;
    }
    if (g_infinite_film && plyr_wrk.film_no < 5)
    {
        poss_item[plyr_wrk.film_no] = 99;
    }
    if (g_infinite_spirit_stone)
    {
        poss_item[5] = 99;
    }
    if (g_infinite_herbal_medicine)
    {
        poss_item[6] = 99;
    }
    if (g_infinite_sacred_water)
    {
        poss_item[7] = 99;
    }
}

void MikuPan_CheatManagerSetInfiniteFilm(int enabled)
{
    g_infinite_film = enabled != 0;
    if (g_infinite_film && plyr_wrk.film_no < 5)
    {
        poss_item[plyr_wrk.film_no] = 99;
    }
}

int MikuPan_CheatManagerInfiniteFilmEnabled(void)
{
    return g_infinite_film ? 1 : 0;
}

void MikuPan_CheatManagerSetInfiniteSpiritStone(int enabled)
{
    g_infinite_spirit_stone = enabled != 0;
    if (g_infinite_spirit_stone)
    {
        poss_item[5] = 99;
    }
}

int MikuPan_CheatManagerInfiniteSpiritStoneEnabled(void)
{
    return g_infinite_spirit_stone ? 1 : 0;
}

void MikuPan_CheatManagerSetInfiniteHerbalMedicine(int enabled)
{
    g_infinite_herbal_medicine = enabled != 0;
    if (g_infinite_herbal_medicine) poss_item[6] = 99;
}
int MikuPan_CheatManagerInfiniteHerbalMedicineEnabled(void)
{
    return g_infinite_herbal_medicine ? 1 : 0;
}
void MikuPan_CheatManagerSetInfiniteSacredWater(int enabled)
{
    g_infinite_sacred_water = enabled != 0;
    if (g_infinite_sacred_water) poss_item[7] = 99;
}
int MikuPan_CheatManagerInfiniteSacredWaterEnabled(void)
{
    return g_infinite_sacred_water ? 1 : 0;
}
void MikuPan_CheatManagerSetOneHitKills(int enabled)
{
    g_one_hit_kills = enabled != 0;
}
int MikuPan_CheatManagerOneHitKillsEnabled(void)
{
    return g_one_hit_kills ? 1 : 0;
}
int MikuPan_CheatManagerForceEnemyLethalDamage(int damage)
{
    return g_one_hit_kills && damage > 0 ? 0x7fffffff : damage;
}
void MikuPan_CheatManagerSetMaxCameraRange(int enabled)
{
    g_max_camera_range = enabled != 0;
    if (g_max_camera_range) cam_custom_wrk.charge_range = 3;
}
int MikuPan_CheatManagerMaxCameraRangeEnabled(void)
{
    return g_max_camera_range ? 1 : 0;
}
void MikuPan_CheatManagerSetMaxCameraSpeed(int enabled)
{
    g_max_camera_speed = enabled != 0;
    if (g_max_camera_speed) cam_custom_wrk.charge_speed = 3;
}
int MikuPan_CheatManagerMaxCameraSpeedEnabled(void)
{
    return g_max_camera_speed ? 1 : 0;
}
void MikuPan_CheatManagerSetUnlockBonusCameraOptions(int enabled)
{
    g_unlock_bonus_camera_options = enabled != 0;
    if (g_unlock_bonus_camera_options)
    {
        for (int i = 0; i < 5; ++i) cam_custom_wrk.func_sub[i] = 2;
        if (cam_custom_wrk.set_sub < 5) cam_custom_wrk.func_sub[cam_custom_wrk.set_sub] = 3;
    }
}
int MikuPan_CheatManagerUnlockBonusCameraOptionsEnabled(void) { return g_unlock_bonus_camera_options ? 1 : 0; }
void MikuPan_CheatManagerSetUnlockSpecialCameraOptions(int enabled)
{
    g_unlock_special_camera_options = enabled != 0;
    if (g_unlock_special_camera_options)
    {
        for (int i = 0; i < 5; ++i) cam_custom_wrk.func_spe[i] = 2;
        if (cam_custom_wrk.set_spe < 5) cam_custom_wrk.func_spe[cam_custom_wrk.set_spe] = 3;
    }
}
int MikuPan_CheatManagerUnlockSpecialCameraOptionsEnabled(void) { return g_unlock_special_camera_options ? 1 : 0; }
void MikuPan_CheatManagerSetMaxCameraUpgrades(int enabled)
{
    g_max_camera_upgrades = enabled != 0;
    if (g_max_camera_upgrades) { cam_custom_wrk.charge_range = 3; cam_custom_wrk.charge_speed = 3; cam_custom_wrk.charge_max = 3; }
}
int MikuPan_CheatManagerMaxCameraUpgradesEnabled(void) { return g_max_camera_upgrades ? 1 : 0; }
void MikuPan_CheatManagerSetMaxCampaignScore(int enabled)
{
    g_max_campaign_score = enabled != 0;
    if (g_max_campaign_score && ingame_wrk.game == 0)
    {
        ingame_wrk.total_point = 9999999;
    }
}
int MikuPan_CheatManagerMaxCampaignScoreEnabled(void) { return g_max_campaign_score ? 1 : 0; }
void MikuPan_CheatManagerSetMaxCameraPoints(int enabled)
{
    g_max_camera_points = enabled != 0;
    if (g_max_camera_points) cam_custom_wrk.point = 9999999;
}
int MikuPan_CheatManagerMaxCameraPointsEnabled(void) { return g_max_camera_points ? 1 : 0; }
void MikuPan_CheatManagerSetAlwaysLowGameTime(int enabled)
{
    g_always_low_game_time = enabled != 0;
    if (g_always_low_game_time && ingame_wrk.game == 0)
    {
        time_wrk.game = 0;
        time_wrk.one_game = 0;
    }
}
int MikuPan_CheatManagerAlwaysLowGameTimeEnabled(void) { return g_always_low_game_time ? 1 : 0; }
void MikuPan_CheatManagerSetFasterRunning(int enabled) { g_faster_running = enabled != 0; }
int MikuPan_CheatManagerFasterRunningEnabled(void) { return g_faster_running ? 1 : 0; }
float MikuPan_CheatManagerScaleRunSpeed(float speed)
{
    return g_faster_running ? speed * 2.0f : speed;
}
void MikuPan_CheatManagerSetFasterWalking(int enabled) { g_faster_walking = enabled != 0; }
int MikuPan_CheatManagerFasterWalkingEnabled(void) { return g_faster_walking ? 1 : 0; }
float MikuPan_CheatManagerScaleWalkSpeed(float speed)
{
    return g_faster_walking ? speed * 2.0f : speed;
}
void MikuPan_CheatManagerSetMaxBattleScore(int enabled)
{
    g_max_battle_score = enabled != 0;
    if (g_max_battle_score && ingame_wrk.game == 1)
    {
        msn_total_score = 99996499;
    }
}
int MikuPan_CheatManagerMaxBattleScoreEnabled(void) { return g_max_battle_score ? 1 : 0; }
void MikuPan_CheatManagerSetMaxBattleHighScore(int enabled)
{
    g_max_battle_high_score = enabled != 0;
    if (g_max_battle_high_score && ingame_wrk.game == 1 && btl_wrk.stage_no < 20)
    {
        msn_high_score = 9999;
    }
}
int MikuPan_CheatManagerMaxBattleHighScoreEnabled(void) { return g_max_battle_high_score ? 1 : 0; }

}
