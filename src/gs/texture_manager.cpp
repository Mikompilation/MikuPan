#include "texture_manager.h"

std::unordered_map<unsigned long long, unsigned char*> texture_atlas;

void AddTexture(unsigned long long tbp0, unsigned char *img)
{
    texture_atlas[tbp0] = img;
}

unsigned char * GetTexture(unsigned long long tbp0)
{
    auto el = texture_atlas.find(tbp0);
    if (el != texture_atlas.end())
    {
        return texture_atlas[tbp0];
    }

    return nullptr;
}