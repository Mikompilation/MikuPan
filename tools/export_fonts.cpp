#include "mikupan/rendering/mikupan_font_identity.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace MikuPanFonts;
std::vector<uint8_t> Read(const fs::path &path,uint64_t offset,size_t size)
{
    if (size>16*1024*1024) throw std::runtime_error("Font pack is too large.");
    std::ifstream f(path,std::ios::binary); std::vector<uint8_t> data(size);
    if (!f || offset>uint64_t(INT64_MAX)) throw std::runtime_error("Cannot open font source.");
    f.seekg(static_cast<std::streamoff>(offset));
    if (!f.read(reinterpret_cast<char *>(data.data()),static_cast<std::streamsize>(size))) throw std::runtime_error("Truncated font source.");
    return data;
}
uint32_t Word(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
void Export(const std::vector<uint8_t> &data,const fs::path &root,const std::string &name)
{
    Atlases atlases; std::string error;
    if (!Decode(data.data(),data.size(),BankTex0,atlases,error)) throw std::runtime_error(name+": "+error);
    const auto hash=Fingerprint(atlases);const auto directory=root/hash;
    std::cout<<name<<" -> "<<hash<<"\n";
    fs::create_directories(root);
    if (!fs::create_directory(directory))
    {
        if (!fs::is_directory(directory)) throw std::runtime_error("Output path is not a directory.");
        std::cout<<"Existing fingerprint folder retained.\n"; return;
    }
    try
    {
        for (size_t bank=0;bank<BankCount;++bank)
        {
            auto &image=atlases[bank];const auto path=directory/("hd_texture_font_"+std::to_string(bank)+".png");
            SDL_Surface *surface=SDL_CreateSurfaceFrom(image.width,image.height,SDL_PIXELFORMAT_RGBA32,image.rgba.data(),image.width*4);
            if (!surface) throw std::runtime_error(SDL_GetError());
            const auto utf8=path.u8string();const bool ok=SDL_SavePNG(surface,reinterpret_cast<const char *>(utf8.c_str()));
            SDL_DestroySurface(surface);
            if (!ok) throw std::runtime_error(SDL_GetError());
        }
    }
    catch (...) { std::error_code ignored;fs::remove_all(directory,ignored);throw; }
}
int main(int argc,char **argv)
{
    try
    {
        fs::path data,out,pk2;std::string region="us";
        for(int i=1;i<argc;i++)
        {
            const std::string option=argv[i];
            if (i+1>=argc) throw std::runtime_error("Missing option value.");
            const char *value=argv[++i];
            if(option=="--data") data=fs::u8path(value);
            else if(option=="--pk2") pk2=fs::u8path(value);
            else if(option=="--out") out=fs::u8path(value);
            else if(option=="--region") region=value;
            else throw std::runtime_error("Unknown option.");
        }
        if(out.empty() || data.empty()==pk2.empty() || (region!="us"&&region!="eu"))
            throw std::runtime_error("Usage: MikuPanFontTool --data GAME_FOLDER --region us|eu --out OUTPUT_ROOT\n       MikuPanFontTool --pk2 FONT.pk2 --out OUTPUT_ROOT");
        if(!pk2.empty()) {Export(Read(pk2,0,size_t(fs::file_size(pk2))),out,pk2.filename().string());return 0;}
        const int first=region=="eu"?172:68,count=region=="eu"?5:1;
        const char *languages[]={"e","f","g","s","i"};
        for(int language=0;language<count;++language)
        {
            const std::string name=region=="eu"?std::string("bin/tim/fnt001_")+languages[language]+".pk2":"bin/tim/fnt001.pk2";
            const auto row=Read(data/"IMG_HD.BIN",uint64_t(first+language)*8,8);
            const uint32_t sector=Word(row.data()),size=Word(row.data()+4);
            if(!size || size>16*1024*1024) throw std::runtime_error("Invalid font size in IMG_HD.BIN; check the selected region.");
            std::vector<uint8_t> bytes;
            if(fs::is_regular_file(data/fs::u8path(name)))
            {
                const auto loose=std::min<uint64_t>(size,fs::file_size(data/fs::u8path(name)));
                if(loose) {bytes=Read(data/fs::u8path(name),0,size_t(loose));bytes.resize(size);}
            }
            if(bytes.empty()) bytes=Read(data/"IMG_BD.BIN",uint64_t(sector)*2048,size);
            Export(bytes,out,name);
        }
        return 0;
    }
    catch(const std::exception &error){std::cerr<<"Font export failed: "<<error.what()<<"\n";return 1;}
}
