#include "common.h"
#include "ig_camra.h"
#include "main/glob.h"

void CameraCustomNewgameInit()
{    
    cam_custom_wrk = (CAM_CUSTOM_WRK){0};

    cam_custom_wrk.set_spe = 0xff;
    cam_custom_wrk.set_sub = 0xff;

    cam_custom_wrk.func_sub[0] = 1;
    cam_custom_wrk.func_sub[1] = 1;
    cam_custom_wrk.func_sub[2] = 1;    
    cam_custom_wrk.func_sub[3] = 1;
    cam_custom_wrk.func_sub[4] = 1;

    cam_custom_wrk.func_spe[0] = 0;
    cam_custom_wrk.func_spe[1] = 0;
    cam_custom_wrk.func_spe[2] = 0;
    cam_custom_wrk.func_spe[3] = 0;    
    cam_custom_wrk.func_spe[4] = 0;
}

void CameraCustomInit()
{
}

void CameraCustomMain()
{
}

void CameraCustomMenuSlct(char* err)
{
}

void CameraCustomFilm(char* err)
{
}

void CameraCustomPowerUp(char* err)
{
}

void CameraCustomSubAbility(char* err)
{
}

void CameraCustomSpecialAbility(char* err)
{
}

void CameraDspCtrl(u_char err)
{
}

void CameraDsp(short int pos_x, short int pos_y, u_char alp, u_char msg)
{
}

char StrKndChk(u_char err)
{
}

char FilmPossChk(u_char knd)
{
}

void CameraModeInOut()
{
}

void CameraModeInOut2()
{
}

void CameraCntRenew()
{
}

int isCameraTopMenu()
{
}

void OutGameInitCamera()
{
}

int isCameraEnd()
{
}

float GetCamDispAlpha()
{
}

void OpenCameraMenu()
{
}

float NeonAlp(short int num, short int exe, short int dly, short int stp, short int no, short int* timer)
{
}

float BeMax(u_char no)
{
}
