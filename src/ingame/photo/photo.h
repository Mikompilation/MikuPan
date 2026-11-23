#include "mikupan/mikupan_region.h"
#ifndef INGAME_PHOTO_PHOTO_H
#define INGAME_PHOTO_PHOTO_H

#include "photo_types.h"
#include "typedefs.h"

#include "ingame/menu/ig_rank.h"


MIKUPAN_NAMESPACE_BEGIN

long long PhotoScoreCount();
int SubjectScoreCount(SUBJECT_WRK *sj);
int SpecialPhotoMakeJudge();
void AddPhotoData();
int GetSavePhotoNo();
void DeletePhotoData(u_char no);
void GetRecordSubject(PICTURE_WRK *pic);


MIKUPAN_NAMESPACE_END
#endif// INGAME_PHOTO_PHOTO_H
