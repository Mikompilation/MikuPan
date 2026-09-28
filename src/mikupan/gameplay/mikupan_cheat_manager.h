#ifndef MIKUPAN_CHEAT_MANAGER_H
#define MIKUPAN_CHEAT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void MikuPan_CheatManagerSetInfiniteHealth(int enabled);
int MikuPan_CheatManagerInfiniteHealthEnabled(void);
void MikuPan_CheatManagerUpdatePlayer(void);
void MikuPan_CheatManagerSetInfiniteFilm(int enabled);
int MikuPan_CheatManagerInfiniteFilmEnabled(void);
void MikuPan_CheatManagerSetInfiniteSpiritStone(int enabled);
int MikuPan_CheatManagerInfiniteSpiritStoneEnabled(void);
void MikuPan_CheatManagerSetInfiniteHerbalMedicine(int enabled);
int MikuPan_CheatManagerInfiniteHerbalMedicineEnabled(void);
void MikuPan_CheatManagerSetInfiniteSacredWater(int enabled);
int MikuPan_CheatManagerInfiniteSacredWaterEnabled(void);
void MikuPan_CheatManagerSetOneHitKills(int enabled);
int MikuPan_CheatManagerOneHitKillsEnabled(void);
void MikuPan_CheatManagerSetMaxCameraRange(int enabled);
int MikuPan_CheatManagerMaxCameraRangeEnabled(void);
void MikuPan_CheatManagerSetMaxCameraSpeed(int enabled);
int MikuPan_CheatManagerMaxCameraSpeedEnabled(void);
void MikuPan_CheatManagerSetUnlockBonusCameraOptions(int enabled);
int MikuPan_CheatManagerUnlockBonusCameraOptionsEnabled(void);
void MikuPan_CheatManagerSetUnlockSpecialCameraOptions(int enabled);
int MikuPan_CheatManagerUnlockSpecialCameraOptionsEnabled(void);
void MikuPan_CheatManagerSetMaxCameraUpgrades(int enabled);
int MikuPan_CheatManagerMaxCameraUpgradesEnabled(void);
void MikuPan_CheatManagerSetMaxCampaignScore(int enabled);
int MikuPan_CheatManagerMaxCampaignScoreEnabled(void);
void MikuPan_CheatManagerSetMaxCameraPoints(int enabled);
int MikuPan_CheatManagerMaxCameraPointsEnabled(void);
void MikuPan_CheatManagerSetAlwaysLowGameTime(int enabled);
int MikuPan_CheatManagerAlwaysLowGameTimeEnabled(void);
void MikuPan_CheatManagerSetFasterRunning(int enabled);
int MikuPan_CheatManagerFasterRunningEnabled(void);
float MikuPan_CheatManagerScaleRunSpeed(float speed);
void MikuPan_CheatManagerSetFasterWalking(int enabled);
int MikuPan_CheatManagerFasterWalkingEnabled(void);
float MikuPan_CheatManagerScaleWalkSpeed(float speed);
void MikuPan_CheatManagerSetMaxBattleScore(int enabled);
int MikuPan_CheatManagerMaxBattleScoreEnabled(void);
void MikuPan_CheatManagerSetMaxBattleHighScore(int enabled);
int MikuPan_CheatManagerMaxBattleHighScoreEnabled(void);
int MikuPan_CheatManagerForceEnemyLethalDamage(int damage);

#ifdef __cplusplus
}
#endif

#endif
