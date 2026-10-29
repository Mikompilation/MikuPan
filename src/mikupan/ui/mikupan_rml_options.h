#ifndef MIKUPAN_RML_OPTIONS_H
#define MIKUPAN_RML_OPTIONS_H

#ifdef __cplusplus
namespace Rml
{
class Context;
}

bool MikuPan_RmlOptionsInit(Rml::Context* context);
void MikuPan_RmlOptionsStartFrame(void);
void MikuPan_RmlOptionsPrepareShutdown(void);
void MikuPan_RmlOptionsShutdown(void);
extern "C" {
#endif

void MikuPan_RmlOptionsOpen(void);
void MikuPan_RmlOptionsOpenInGame(void);
void MikuPan_RmlOptionsClose(void);
int MikuPan_RmlOptionsConsumeExitRequest(void);
int MikuPan_RmlOptionsIsOpen(void);
int MikuPan_RmlOptionsIsCalibrationOpen(void);
int MikuPan_RmlOptionsInputBlocked(void);
int MikuPan_RmlOptionsBindingCaptureActive(void);
void MikuPan_RmlOptionsRequestExit(void);
void MikuPan_RmlOptionsHandleCancel(void);
int MikuPan_RmlOptionsActivateFocusedControl(void);
/* Square / X: clears the binding under the cursor. */
void MikuPan_RmlOptionsHandleAltConfirm(void);
/* L1 / R1: the previous or next section. */
void MikuPan_RmlOptionsHandleSection(int direction);
int MikuPan_RmlOptionsHandleHorizontalInput(int direction);
int MikuPan_RmlOptionsHandleVerticalInput(int direction);
/* Which device the prompts name: nonzero for the controller. */
void MikuPan_RmlOptionsNoteInputDevice(int pad);
void MikuPan_RmlOptionsApplyTheme(int theme);
void MikuPan_RmlOptionsApplyFont(int font);
void MikuPan_RmlOptionsApplyLanguage(int language);
void MikuPan_RmlOptionsToggleDebug(void);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_RML_OPTIONS_H
