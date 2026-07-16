#ifndef MIKUPAN_RML_SAVE_LOAD_H
#define MIKUPAN_RML_SAVE_LOAD_H

#ifdef __cplusplus
namespace Rml
{
class Context;
}

bool MikuPan_RmlSaveLoadInit(Rml::Context* context);
void MikuPan_RmlSaveLoadStartFrame(void);
void MikuPan_RmlSaveLoadPrepareShutdown(void);
void MikuPan_RmlSaveLoadShutdown(void);
extern "C" {
#endif

#define MIKUPAN_RML_SAVE_LOAD_RESULT_NONE 0
#define MIKUPAN_RML_SAVE_LOAD_RESULT_SAVED 1
#define MIKUPAN_RML_SAVE_LOAD_RESULT_LOADED 2
#define MIKUPAN_RML_SAVE_LOAD_RESULT_CANCELLED 3

void MikuPan_RmlSaveLoadOpenSave(int mission_flag);
void MikuPan_RmlSaveLoadOpenLoad(void);
void MikuPan_RmlSaveLoadClose(void);
int MikuPan_RmlSaveLoadIsOpen(void);
int MikuPan_RmlSaveLoadHandleVerticalInput(int direction);
int MikuPan_RmlSaveLoadHandleHorizontalInput(int direction);
int MikuPan_RmlSaveLoadActivate(void);
int MikuPan_RmlSaveLoadHandleCancel(void);
int MikuPan_RmlSaveLoadConsumeResult(void);
void MikuPan_RmlSaveLoadCapturePreview(void);

#ifdef __cplusplus
}
#endif

#endif
