#ifndef OS_KEY_CNF_H
#define OS_KEY_CNF_H

#include "typedefs.h"

#define MIKUPAN_ACTION_PROFILE_MODE_NORMAL (0)
#define MIKUPAN_ACTION_PROFILE_MODE_FINDER (1)
#define MIKUPAN_ACTION_PROFILE_MODE_COUNT  (2)
#define MIKUPAN_ACTION_PROFILE_ACTION_COUNT (16)
#define MIKUPAN_ACTION_PROFILE_TARGET_NONE (-1)

extern u_char key_type[32];

void SetDefaultKeyType();
void SetKeyType();
void MikuPan_ResetCustomActionProfile(void);
void MikuPan_SetCustomActionProfileEnabled(int enabled);
int MikuPan_IsCustomActionProfileEnabled(void);
void MikuPan_SetCustomActionProfileSubjectiveMove(int enabled);
int MikuPan_CustomActionProfileUsesSubjectiveMove(void);
void MikuPan_SetCustomActionProfileDpadSubjectiveMove(int enabled);
int MikuPan_CustomActionProfileUsesDpadSubjectiveMove(void);
void MikuPan_SetCustomActionProfileStickSubjectiveMove(int enabled);
int MikuPan_CustomActionProfileUsesStickSubjectiveMove(void);
void MikuPan_SetCustomActionProfileFinderReverseY(int enabled);
int MikuPan_CustomActionProfileUsesFinderReverseY(void);
void MikuPan_SetCustomActionProfileFinderSwapSticks(int enabled);
int MikuPan_CustomActionProfileSwapsFinderSticks(void);
void MikuPan_SetFinderDpadFilmSwapEnabled(int enabled);
int MikuPan_FinderDpadFilmSwapEnabled(void);
int MikuPan_KeyProfileUsesSubjectiveMove(void);
int MikuPan_KeyProfileUsesFinderReverseY(void);
int MikuPan_KeyProfileSwapsFinderSticks(void);
int MikuPan_KeyProfileUsesFinderShoulderToggle(void);
int MikuPan_GetCustomActionProfileTarget(int mode, int action);
void MikuPan_SetCustomActionProfileTarget(int mode, int action, int target);
int MikuPan_GetDefaultActionProfileTarget(int action);
const char *MikuPan_ActionProfileModeLabel(int mode);
const char *MikuPan_ActionProfileActionLabel(int mode, int action);
const char *MikuPan_ActionProfileTargetLabel(int target);

#endif // OS_KEY_CNF_H
