#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_SCENE_H
#define OS_EEIOP_ADPCM_EA_SCENE_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmSceneMain();
void AdpcmPreSceneFadeOut(u_short fout_flm);
u_char IsFadeoutEndAdpcmScene();
void AdpcmScenePreLoadReq(int scene_no);
int IsAdpcmScenePreLoadEnd();
void AdpcmScenePreLoadEndPlay(int scene_no);
void AdpcmSceneStop();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_SCENE_H
