#include "common.h"
#include "pht_make.h"


#include "data/hintdat.h"
#include "data/hintreadat.h"
#include "data/pazene_load_tbl.h"

/* sdata 357220 */ int frm_type = 2;
/* sdata 357224 */ int ers_type = 1;
/* sdata 357244 */ int hint_2d = 0;
/* sdata 357248 */ int hint_rea = 0;
/* sdata 35724c */ int hint_3d = 0;
/* sdata 357250 */ int hint_2d_num = 0;
/* sdata 357254 */ int hint_3d_num = 0;
/* sdata 357258 */ int pic_save_flag = 0;
/* sdata 35725c */ int pht_finder_off = 0;

void InitPhotoMake()
{
}

void CopyScreenToBuffer(int addr, int szfl, int mx, int my, int mw, int mh)
{
}

void DrawPhotoBuffer(u_int pri, int addr, int szfl, int x, int y, int szw, int szh, int mszw, int mszh, int ftype,
    int alp, int ztype)
{
}

void CompressData(int addri, int addro, int n)
{
}

void UncompressData(int addri, int n, int addro)
{
}

void TakePhotoFromScreen()
{
}

void MakeSPhotoFromWorkArea(int n)
{
}

void MakeSPhotoFromCompress(int ni, int no)
{
}

void DrawSPhotoFromSmallPhotoArea(int n, int pri, int ftype, int x, int y, u_char alp)
{
}

void DrawSPhotoFromSmallPhotoAreaAD(int addr, int n, int pri, int ftype, int x, int y, u_char alp)
{
}

void DrawSPhotoFromSmallPhotoArea2(int n, int pri, int ftype, int x, int y, int szw, int szh, u_char alp)
{
}

void DrawSPhotoFromSmallPhotoArea2AD(int addr, int n, int pri, int ftype, int x, int y, int szw, int szh, int alp)
{
}

void CompPhotoFromWorkArea(int n)
{
}

void DrawPhotoFromWorkArea(int pri, int ftype, int x, int y, int szw, int szh, u_char alp)
{
}

void DrawPhotoFromCompress(int n, u_int pri, int ftype, int x, int y, int szw, int szh, u_char alp)
{
}

void UncompressPhoto(int n)
{
}

void UncompressPhotoAD(int addr, int n)
{
}

void DrawPhotoFromPhotoWrk(int n, u_int pri, int ftype, int x, int y, int szw, int szh, u_char alp)
{
}

void CopyPhoto(int addri, int ni, int addro, int no)
{
}

void CopySPhoto(int addri, int ni, int addro, int no)
{
}

void DrawPhotoHinttex(u_int pri, int num)
{
}

void DrawPhotoHinttex2(u_int sw, u_int pri, int num)
{
}

void DrawPhotoEffect(u_int pri, int type)
{
}

void DrawPhotoFilter(u_int pri, int type, u_char alp)
{
}

void DispPhotoFrame0()
{
}

void DispPhotoFrame00(int flag)
{
}

int DispPhotoFrame1(int fl)
{
    return 0;
}

void DispPhotoFrame2()
{
}

void DispPhotoFrame2_2(int type)
{
}

void DispPhotoFrame2_3(int type)
{
}

void SetNegaFilter(int type)
{
}

void PhotoMake()
{
}

void PhotoMake_EneDead()
{
}

void PhotoMakeSaveInit()
{
}

void DrawPicture(int pri, int addr, int n, float x, float y, float szw, float szh, u_char alp)
{
}

void CopyPicture(int addri, int ni, int addro, int no)
{
}

void ReqSavePicture()
{
}

void PhotoMakeSave()
{
}

void PhotoMakeSave2()
{
}
