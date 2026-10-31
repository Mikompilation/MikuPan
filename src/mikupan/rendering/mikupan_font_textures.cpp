#include "mikupan_font_textures.h"
#include "mikupan_font_identity.h"
#include "mikupan_gpu.h"
#include "../io/mikupan_file.h"
#include "../mikupan_memory.h"
#include "graphics/graph2d/message.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "enums.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>
extern char *file_name[];
namespace MikuPanFonts
{
namespace
{
struct Set
{
    std::string hash;
    std::array<int, BankCount> width{},height{};
    std::array<unsigned int, BankCount> texture{};
    std::array<bool, BankCount> tried{};
};
struct Binding { int file, sector, size, set; };
std::vector<Set> sets;
std::vector<Binding> bindings;
struct Pending { bool font=false; int file=-1, set=-1; };
std::array<Pending,32> pending{};
int active = -1, active_file = -1;
bool prepared = false;
uint32_t Word(const unsigned char *p)
{
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
bool ReadFont(int file, Binding &binding, std::vector<uint8_t> &bytes)
{
    unsigned char entry[8];
    if (file<0 || file>=GetFileNameCount() ||
        !MikuPan_ReadDataFileRange("IMG_HD.BIN",int64_t(file)*8,entry,sizeof(entry))) return false;
    const uint32_t sector=Word(entry),size=Word(entry+4);
    if (!size || size>16*1024*1024 || sector>0x7fffffffu) return false;
    binding={file,int(sector),int(size),-1}; bytes.assign(size,0);
    // Font loads use original loose/archive bytes, including translation patches.
    const auto loose=MikuPan_GetDataFileSize(file_name[file]);
    if (loose && MikuPan_ReadDataFileRange(file_name[file],0,bytes.data(),std::min(loose,size))) return true;
    return MikuPan_ReadDataFileRange("IMG_BD.BIN",int64_t(sector)*2048,bytes.data(),size);
}
}
void Prepare()
{
    if (prepared) return;
    prepared=true; ResetLoads();
#ifdef BUILD_EU_VERSION
    constexpr int first=FNT001_E_PK2,count=5;
#else
    constexpr int first=FNT001_PK2,count=1;
#endif
    std::array<uint64_t,BankCount> tex0;
    for (size_t i=0;i<BankCount;++i) tex0[i]=fntdat[i].tex0;
    for (int file=first;file<first+count;++file)
    {
        Binding binding{}; std::vector<uint8_t> bytes; Atlases atlases; std::string error;
        if (!ReadFont(file,binding,bytes) || !Decode(bytes.data(),bytes.size(),tex0,atlases,error))
        {
            SDL_Log("MikuPan: HD fonts disabled for file %d: %s",file,error.empty()?"cannot read original font":error.c_str());
            continue;
        }
        const auto hash=Fingerprint(atlases);
        auto found=std::find_if(sets.begin(),sets.end(),[&](const Set &s){return s.hash==hash;});
        binding.set=int(found-sets.begin());
        if (found==sets.end())
        {
            Set set; set.hash=hash;
            for (size_t i=0;i<BankCount;++i) { set.width[i]=atlases[i].width; set.height[i]=atlases[i].height; }
            sets.push_back(std::move(set));
        }
        bindings.push_back(binding);
        SDL_Log("MikuPan: font %s -> %s",file_name[file],hash.c_str());
    }
}
void ResetLoads() { pending.fill({}); }
void LoadRequested(int id,int file,void *destination,int sector,int size)
{
    if (id<0 || size_t(id)>=pending.size()) return;
    pending[id]={};
    if (destination!=MikuPan_GetHostPointer(FontTextAddress)) return;
    pending[id]={true,file,-1};
    for (const auto &b:bindings)
        if (b.file==file && b.sector==sector && b.size==size) { pending[id].set=b.set; break; }
}
void LoadCompleted(int id)
{
    if (id<0 || size_t(id)>=pending.size()) return;
    if (pending[id].font) { active=pending[id].set; active_file=pending[id].file; }
    pending[id]={};
}
int LoadedLanguage()
{
#ifdef BUILD_EU_VERSION
    return active_file>=FNT001_E_PK2 && active_file<=FNT001_I_PK2 ? active_file-FNT001_E_PK2 : -1;
#else
    return active_file==FNT001_PK2 ? 0 : -1;
#endif
}
unsigned int Texture(int bank,int width,int height)
{
    if (active<0 || size_t(active)>=sets.size() || bank<0 || size_t(bank)>=BankCount) return 0;
    auto &set=sets[active];
    if (width!=set.width[bank] || height!=set.height[bank]) return 0;
    if (set.tried[bank]) return set.texture[bank];
    set.tried[bank]=true;
    const auto relative="resources/fonts/"+set.hash+"/hd_texture_font_"+std::to_string(bank)+".png";
    char path[4096];
    if (!MikuPan_ResolveBasePath(relative.c_str(),path,sizeof(path))) return 0;
    unsigned char header[24]{};
    SDL_IOStream *input=SDL_IOFromFile(path,"rb");
    if (!input) return 0;
    const size_t read=SDL_ReadIO(input,header,sizeof(header));
    SDL_CloseIO(input);
    const auto big=[&](int at){return uint32_t(header[at])<<24 | uint32_t(header[at+1])<<16 | uint32_t(header[at+2])<<8 | header[at+3];};
    const uint32_t w=big(16),h=big(20);
    if (read!=sizeof(header) || std::memcmp(header,"\x89PNG\r\n\x1a\n",8) || w<uint32_t(width) ||
        w>uint32_t(width)*4 || w%width || h!=uint32_t(height)*(w/width))
    {
        SDL_Log("MikuPan: ignored invalid HD font bank %d for %s",bank,set.hash.c_str()); return 0;
    }
    SDL_Surface *surface=SDL_LoadPNG(path);
    if (surface && uint32_t(surface->w)==w && uint32_t(surface->h)==h)
        set.texture[bank]=MikuPan_GPUCreateTextureFromSurface(surface);
    SDL_DestroySurface(surface);
    if (!set.texture[bank]) SDL_Log("MikuPan: could not load HD font bank %d for %s: %s",bank,set.hash.c_str(),SDL_GetError());
    return set.texture[bank];
}
void ShutdownGpu()
{
    for (auto &set:sets)
        for (auto &id:set.texture)
            if (id) { MikuPan_GPUReleaseTexture(id); id=0; }
}
}
