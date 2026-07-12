#ifndef MIKUPAN_GAMEPLAY_MIKUPAN_TITLE_SCENE_H
#define MIKUPAN_GAMEPLAY_MIKUPAN_TITLE_SCENE_H

#include "typedefs.h"
#include "graphics/graph3d/light_types.h"
#include "ingame/camera/camera_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int MikuPan_TitleSceneIsTargetRoom(int msn_no, int room_no);
void MikuPan_TitleSceneBeginFrame(int enabled, int msn_no, int room_no,
                                  const SgCAMERA* camera);
void MikuPan_TitleSceneAmendRoomLightPack(LIGHT_PACK* light_pack);
void MikuPan_TitleSceneDraw(void);

void MikuPan_TitleResetSceneColourState(void);
int MikuPan_TitleUseRoomBackground(void);
void MikuPan_TitleSetUseRoomBackground(int enabled);
int MikuPan_TitleUseDither(void);
void MikuPan_TitleSetUseDither(int enabled);
int MikuPan_TitleDebugWindowVisible(void);
void MikuPan_TitleSetDebugWindowVisible(int enabled);
void MikuPan_TitleAudioPlayBgm(u_short fade_in_frames);
void MikuPan_TitleBackgroundBeginRoomLoad(void);
int MikuPan_TitleBackgroundRoomReady(void);
void MikuPan_TitleBackgroundDrawRoom(void);
void MikuPan_TitleBackgroundDebugUi(void);
void MikuPan_TitleCalibrationPreviewOpen(void);
void MikuPan_TitleCalibrationPreviewDraw(void);

#ifdef __cplusplus
}
#endif

#endif
