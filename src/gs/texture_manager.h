#ifndef MIKUPAN_TEXTURE_MANAGER_H
#define MIKUPAN_TEXTURE_MANAGER_H
#include <unordered_map>

extern std::unordered_map<unsigned long long, unsigned char*> texture_atlas;

extern "C"
{
    void AddTexture(unsigned long long tbp0, unsigned char* img);
    unsigned char* GetTexture(unsigned long long tbp0);
}


#endif //MIKUPAN_TEXTURE_MANAGER_H