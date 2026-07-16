#ifndef MIKUPAN_RML_TITLE_H
#define MIKUPAN_RML_TITLE_H

#ifdef __cplusplus
namespace Rml
{
class Context;
}

bool MikuPan_RmlTitleInit(Rml::Context* context);
void MikuPan_RmlTitleStartFrame(void);
void MikuPan_RmlTitlePrepareShutdown(void);
void MikuPan_RmlTitleShutdown(void);
extern "C" {
#endif

#define MIKUPAN_RML_TITLE_COMMAND_NONE 0
#define MIKUPAN_RML_TITLE_COMMAND_PRESS_START 1
#define MIKUPAN_RML_TITLE_COMMAND_NEW_GAME 2
#define MIKUPAN_RML_TITLE_COMMAND_LOAD_GAME 3
#define MIKUPAN_RML_TITLE_COMMAND_ALBUM 4
#define MIKUPAN_RML_TITLE_COMMAND_SETTINGS 5
#define MIKUPAN_RML_TITLE_COMMAND_EXIT_GAME 6
#define MIKUPAN_RML_TITLE_COMMAND_EXIT_YES 7
#define MIKUPAN_RML_TITLE_COMMAND_EXIT_NO 8

int MikuPan_RmlTitleIsAvailable(void);
void MikuPan_RmlTitleShowPress(float prompt_alpha);
void MikuPan_RmlTitleShowMenu(int selected_index, int menu_count);
void MikuPan_RmlTitleShowExitPrompt(int selected_index);
int MikuPan_RmlTitleConsumeCommand(void);
int MikuPan_RmlTitleConsumeSelection(void);
void MikuPan_RmlTitleNotifySettingsOpened(int selection);
int MikuPan_RmlTitleConsumeSettingsReturnSelection(void);
int MikuPan_RmlTitleIsInputCooldownActive(void);

#ifdef __cplusplus
}
#endif

#endif
