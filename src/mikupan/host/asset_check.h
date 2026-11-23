#pragma once
#include "engine_api.h"
#include <filesystem>
#include <string>

using MikuPanHost::Region;
bool InspectAssets(const std::filesystem::path& root, Region& region, std::string& error);
