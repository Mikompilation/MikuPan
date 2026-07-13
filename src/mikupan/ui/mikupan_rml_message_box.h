#pragma once

namespace Rml
{
class RenderInterface;
}

bool MikuPan_RmlMessageBoxInit(Rml::RenderInterface* render_interface);
void MikuPan_RmlMessageBoxShutdown(void);
