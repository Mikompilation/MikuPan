#ifndef MIKUPAN_RML_DATA_SETUP_H
#define MIKUPAN_RML_DATA_SETUP_H

/// The screen shown when the game data cannot be found. It asks for a disc
/// image, or for IMG_HD.BIN and IMG_BD.BIN, and the game starts by itself as
/// soon as the choice is good: the frame loop polls for the data.

#ifdef __cplusplus
namespace Rml
{
class Context;
}

bool MikuPan_RmlDataSetupInit(Rml::Context* context);
void MikuPan_RmlDataSetupStartFrame(void);
void MikuPan_RmlDataSetupPrepareShutdown(void);
void MikuPan_RmlDataSetupShutdown(void);
int MikuPan_RmlDataSetupIsOpen(void);
void MikuPan_RmlDataSetupHandleVerticalInput(int direction);
void MikuPan_RmlDataSetupActivate(void);
void MikuPan_RmlDataSetupHandleCancel(void);
extern "C" {
#endif

int MikuPan_RmlDataSetupIsAvailable(void);
/// Call every frame the screen should stay up.
void MikuPan_RmlDataSetupShow(void);

#ifdef __cplusplus
}
#endif

#endif
