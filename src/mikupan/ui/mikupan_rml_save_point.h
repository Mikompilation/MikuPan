#ifndef MIKUPAN_RML_SAVE_POINT_H
#define MIKUPAN_RML_SAVE_POINT_H

#ifdef __cplusplus
namespace Rml
{
class Context;
}

bool MikuPan_RmlSavePointInit(Rml::Context* context);
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum MikuPanRmlSavePointAction
{
    MIKUPAN_RML_SAVE_POINT_ACTION_NONE = -1,
    MIKUPAN_RML_SAVE_POINT_ACTION_SAVE = 0,
    MIKUPAN_RML_SAVE_POINT_ACTION_ALBUM = 1,
    MIKUPAN_RML_SAVE_POINT_ACTION_FILM = 2,
    MIKUPAN_RML_SAVE_POINT_ACTION_EXIT = 3
};

void MikuPan_RmlSavePointStartFrame(void);
void MikuPan_RmlSavePointPrepareShutdown(void);
void MikuPan_RmlSavePointShutdown(void);
int MikuPan_RmlSavePointOpen(int availability_mode,
                             int mission_mode,
                             int selection);
void MikuPan_RmlSavePointClose(void);
int MikuPan_RmlSavePointIsOpen(void);
int MikuPan_RmlSavePointInputEnabled(void);
void MikuPan_RmlSavePointSetPresentation(int availability_mode,
                                         int mission_mode,
                                         int interactive,
                                         float opacity);
void MikuPan_RmlSavePointHandleHorizontalInput(int direction);
void MikuPan_RmlSavePointHandleVerticalInput(int direction);
void MikuPan_RmlSavePointActivate(void);
void MikuPan_RmlSavePointHandleCancel(void);
int MikuPan_RmlSavePointConsumeAction(void);
int MikuPan_RmlSavePointGetSelection(void);

#ifdef __cplusplus
}
#endif

#endif
