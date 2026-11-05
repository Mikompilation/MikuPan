#include "mikupan/mikupan_region.h"
namespace Rml
{
class Context;
}
#ifndef MIKUPAN_RML_MODE_SELECT_H
#define MIKUPAN_RML_MODE_SELECT_H


MIKUPAN_NAMESPACE_BEGIN

#ifdef __cplusplus


bool MikuPan_RmlModeSelectInit(Rml::Context* context);
void MikuPan_RmlModeSelectStartFrame(void);
void MikuPan_RmlModeSelectPrepareShutdown(void);
void MikuPan_RmlModeSelectShutdown(void);
MIKUPAN_EXTERN_C {
#endif

#define MIKUPAN_RML_MODE_SELECT_COMMAND_NONE 0
#define MIKUPAN_RML_MODE_SELECT_COMMAND_CONFIRM 1
#define MIKUPAN_RML_MODE_SELECT_COMMAND_LEFT 2
#define MIKUPAN_RML_MODE_SELECT_COMMAND_RIGHT 3

int MikuPan_RmlModeSelectIsAvailable(void);
void MikuPan_RmlModeSelectShowMain(int selected_index, float opacity);
void MikuPan_RmlModeSelectShowStory(int selected_index,
                                    int editing,
                                    int first_enabled_index,
                                    int chapter,
                                    int difficulty,
                                    int costume,
                                    float opacity);
int MikuPan_RmlModeSelectConsumeSelection(void);
int MikuPan_RmlModeSelectConsumeCommand(void);
void MikuPan_RmlModeSelectApplyLanguage(int language);

#ifdef __cplusplus
}
#endif


MIKUPAN_NAMESPACE_END
#endif
