#include "common.h"
#include "wan_soul.h"

WANDER_SOUL_WRK wander_soul_wrk[1];

sceVu0FVECTOR* SetFirstDestination(sceVu0FVECTOR* destination, float* pos)
{
}

float GetDist3(float* v1, float* v2)
{
}

float GetDistPlyr(float* plyr, float* soul)
{
}

void* SetEffectsWithScale(float* pos, SOUL_EFF_PARAM* param, float scale, float rgb)
{
}

int GetUsableWanSoWrkID()
{
}

void SetNewSpeed(WANDER_SOUL_WRK* wswrk)
{
}

void NewMissionInitWanderSoul()
{
}

void ReviveWanderSoul()
{
}

void BSplinePos(float* pos, float* mat_point_array[4], float t1)
{
}

void ClearWanderSoulOne()
{
}

void CallWanderSoul(u_char data_id, float* pos)
{
}

void RegisterWansoEffect()
{
}

void WansoLateInit()
{
}

void ReleaseWanderSoul(u_char data_id)
{
}

int WanSoNearJudge(float dist, float distv)
{
}

int WanSoTouchJudge(float* soul_pos, MOVE_BOX* mbp)
{
}

void SoulFloating(float* pos, float* rgb)
{
}

void SetSoulNewMove()
{
}

void Change2WanSoExtinct()
{
}

void FinishWanSoWrk()
{
}

void ComeOnSignal(float* scale, float* rgb, u_short time)
{
}

float StopAbleDist(float speed, float accel, u_short* count)
{
}

float GetRot(float* a, float* b)
{
}

int RotCheck2(float rot1, float rot2, u_short range)
{
}

void GetBSplinePos(float* pos, float t1)
{
}

int SetMoveDirect(float s2p_len)
{
}

void AddVec2MatD(float* mat[4], float* vec)
{
}

void Change2WanSoFloat()
{
}

float GetKnotDist(float* mat[4])
{
}

void SetWansoSpeed(float dist)
{
}

void WansoPreTell()
{
}

u_char WansoAlphaCtrl(float dist)
{
}

void WansoExtinctCtrl()
{
}

void OneSoulCtrl(float* srate, float* arate)
{
}

void WanderSoulCtrl()
{
}

u_char CallSoulTellingCamera(float* soul_pos, float* relative_camera_pos, float* relative_interest,
    float* relative_soul_pos)
{
}

void CallSoulTellingCameraIn(float* soul_pos, float* mic_eye_pos, int time)
{
}

int SoulTellingCameraInCtrl()
{
}
