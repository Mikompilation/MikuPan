#ifndef MIKUPAN_RML_OPTIONS_H
#define MIKUPAN_RML_OPTIONS_H

#ifdef __cplusplus
namespace Rml
{
class Context;
}

bool MikuPan_RmlOptionsInit(Rml::Context* context);
void MikuPan_RmlOptionsStartFrame(void);
void MikuPan_RmlOptionsShutdown(void);
extern "C" {
#endif

void MikuPan_RmlOptionsOpen(void);
void MikuPan_RmlOptionsClose(void);
int MikuPan_RmlOptionsConsumeExitRequest(void);
int MikuPan_RmlOptionsIsOpen(void);
void MikuPan_RmlOptionsRequestExit(void);
void MikuPan_RmlOptionsHandleCancel(void);
int MikuPan_RmlOptionsAnySelectOpen(void);
int MikuPan_RmlOptionsActivateFocusedControl(void);
int MikuPan_RmlOptionsHandleHorizontalInput(int direction);
int MikuPan_RmlOptionsHandleVerticalInput(int direction);
void MikuPan_RmlOptionsEnsureFocusedControlVisible(void);
int MikuPan_RmlOptionsConsumeMoveSoundRequest(void);
void MikuPan_RmlOptionsToggleDebug(void);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_RML_OPTIONS_H
