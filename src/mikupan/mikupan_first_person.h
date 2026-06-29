#ifndef MIKUPAN_MIKUPAN_FIRST_PERSON_H
#define MIKUPAN_MIKUPAN_FIRST_PERSON_H

#include "typedefs.h"
#include "ingame/enemy/move_types.h"

extern int mikupan_first_person_enabled;
extern int mikupan_first_person_r3_toggle_enabled;
extern int mikupan_first_person_auto_run_enabled;
extern float mikupan_first_person_eye_height;
extern float mikupan_first_person_eye_forward;
extern float mikupan_first_person_look_distance;
extern float mikupan_first_person_fov_deg;
extern float mikupan_first_person_stick_yaw_speed_deg;
extern float mikupan_first_person_stick_pitch_speed_deg;

int MikuPan_FirstPersonEnabled(void);
int MikuPan_FirstPersonActive(void);
void MikuPan_FirstPersonSetEnabled(int enabled);
void MikuPan_FirstPersonToggle(void);
void MikuPan_FirstPersonBeginFrame(void);
void MikuPan_FirstPersonUpdateToggle(void);
int MikuPan_FirstPersonCameraCtrl(void);
int MikuPan_FirstPersonMoveCtrl(MOVE_BOX* mb, sceVu0FVECTOR tv);
int MikuPan_FirstPersonShouldBlockSpotMove(void);
int MikuPan_FirstPersonShouldHidePlayerModel(void);
int MikuPan_FirstPersonUseFinderFog(void);
int MikuPan_FirstPersonShouldUseCameraSpotlight(void);

#endif // MIKUPAN_MIKUPAN_FIRST_PERSON_H
