#include "mikupan/mikupan_region.h"
namespace Rml
{
class RenderInterface;
}
#pragma once




MIKUPAN_NAMESPACE_BEGIN

bool MikuPan_RmlMessageBoxInit(Rml::RenderInterface* render_interface);
void MikuPan_RmlMessageBoxShutdown(void);

MIKUPAN_NAMESPACE_END
