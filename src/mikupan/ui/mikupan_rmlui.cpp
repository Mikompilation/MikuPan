#include "mikupan/ui/mikupan_rmlui.h"
#include "mikupan/ui/mikupan_rml_options.h"
#include "mikupan/ui/mikupan_rml_save_load.h"
#include "mikupan/ui/mikupan_rml_save_point.h"
#include "mikupan/ui/mikupan_rml_title.h"
#include "mikupan/ui/mikupan_ui.h"

#include "enums.h"
#include "glad/gl.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph2d/tim2.h"
#include "ingame/info/inf_disp.h"
#include "mikupan/io/mikupan_file.h"
#include "mikupan/io/mikupan_controller.h"
#include "mikupan/mikupan_memory.h"

#include "mikupan/rendering/mikupan_shader.h"
#include "mikupan/rendering/mikupan_pipeline.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_renderer_internal.h"
#include "mikupan/rendering/mikupan_gpu.h"

#include "SDL3/SDL.h"
#include "RmlUi/Core.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/SystemInterface.h"
#include "RmlUi/Core/Types.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

int SeStartFix(int se_no, unsigned short fin_spd, unsigned short vol_max,
               unsigned short pitch, unsigned char menu);

namespace
{

struct MikuPanRmlGeometry
{
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
};

enum class MikuPanTm2TextureMode
{
    Standard,
    Header
};

bool ParseMikuPanTm2Source(const std::string& source,
                           int& texture_index,
                           MikuPanTm2TextureMode& mode)
{
    static constexpr std::string_view standard_scheme = "mikupan-tm2:";
    static constexpr std::string_view header_scheme = "mikupan-tm2-header:";
    static constexpr std::string_view package = "pl_stts/";

    size_t scheme_offset = source.find(header_scheme);
    if (scheme_offset != std::string::npos)
    {
        mode = MikuPanTm2TextureMode::Header;
    }
    else
    {
        scheme_offset = source.find(standard_scheme);
        if (scheme_offset == std::string::npos)
        {
            return false;
        }
        mode = MikuPanTm2TextureMode::Standard;
    }

    const size_t package_offset = source.find(package, scheme_offset);
    if (package_offset == std::string::npos)
    {
        return false;
    }

    const size_t index_begin = package_offset + package.size();
    const size_t extension = source.find(".tm2", index_begin);
    if (extension == std::string::npos || extension == index_begin)
    {
        return false;
    }

    const char* begin = source.data() + index_begin;
    const char* end = source.data() + extension;
    const auto result = std::from_chars(begin, end, texture_index);
    return result.ec == std::errc() && result.ptr == end && texture_index >= 0;
}

bool ParseMikuPanSpriteSource(const std::string& source, int& sprite_index)
{
    static constexpr std::string_view scheme = "mikupan-sprite:";
    const size_t scheme_offset = source.find(scheme);
    if (scheme_offset == std::string::npos)
    {
        return false;
    }

    size_t index_begin = scheme_offset + scheme.size();
    while (index_begin < source.size() && source[index_begin] == '/')
    {
        index_begin++;
    }

    const char* begin = source.data() + index_begin;
    const char* end = source.data() + source.size();
    const auto result = std::from_chars(begin, end, sprite_index);
    return result.ec == std::errc() && result.ptr == end && sprite_index >= 0;
}

unsigned int LoadMikuPanTm2Texture(int texture_index,
                                   MikuPanTm2TextureMode mode,
                                   Rml::Vector2i& texture_dimensions)
{
    const int64_t package_address =
        MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS);
    if (package_address <= 0)
    {
        return 0;
    }

    const auto* package = reinterpret_cast<const uint32_t*>(package_address);
    const int texture_count = static_cast<int>(package[0]);
    if (texture_count <= 0 || texture_count > 256
        || texture_index >= texture_count)
    {
        return 0;
    }

    static constexpr uint32_t ps2_memory_size = 32u * 1024u * 1024u;
    static constexpr uint32_t package_address_offset = PL_STTS_PK2_ADDRESS;
    static constexpr uint32_t package_limit =
        ps2_memory_size - package_address_offset;
    const uint32_t offset = package[4 + texture_index];
    if (offset < 16 || offset >= package_limit)
    {
        return 0;
    }

    void* tim2 = reinterpret_cast<void*>(package_address + offset);
    if (Tim2CheckFileHeaer(tim2) == 0)
    {
        return 0;
    }

    TIM2_PICTUREHEADER* picture = Tim2GetPictureHeader(tim2, 0);
    if (picture == nullptr || picture->TotalSize == 0
        || picture->TotalSize > package_limit - offset
        || picture->ImageWidth == 0 || picture->ImageHeight == 0
        || picture->ImageWidth > 4096 || picture->ImageHeight > 4096)
    {
        return 0;
    }

    const int source_width = picture->ImageWidth;
    const int source_height = picture->ImageHeight;
    int output_width = source_width;
    int output_height = source_height;
    if (mode == MikuPanTm2TextureMode::Header)
    {
        output_width = 205;
        output_height = 28;
    }

    std::vector<unsigned char> pixels(
        static_cast<size_t>(output_width)
        * static_cast<size_t>(output_height) * 4u);

    for (int y = 0; y < output_height; y++)
    {
        const int source_y = y % source_height;
        for (int x = 0; x < output_width; x++)
        {
            const int source_x = x % source_width;
            const u_int color =
                Tim2GetTextureColor(picture, 0, 0, source_x, source_y);
            const size_t destination =
                static_cast<size_t>((y * output_width + x) * 4);

            unsigned int alpha =
                std::min(255u, ((color >> 24) & 0xff) * 2u);
            if (mode == MikuPanTm2TextureMode::Header)
            {
                float fade = 1.0f;
                if (x < 18)
                {
                    fade = static_cast<float>(x) / 17.0f;
                }
                else if (x >= 135)
                {
                    fade = 1.0f
                        - (static_cast<float>(x - 135) / 69.0f);
                }
                alpha = static_cast<unsigned int>(
                    static_cast<float>(alpha) * std::clamp(fade, 0.0f, 1.0f));
            }

            const float brightness =
                mode == MikuPanTm2TextureMode::Header ? 0.82f : 1.0f;
            pixels[destination + 0] = static_cast<unsigned char>(
                static_cast<float>(color & 0xff) * brightness);
            pixels[destination + 1] = static_cast<unsigned char>(
                static_cast<float>((color >> 16) & 0xff) * brightness);
            pixels[destination + 2] = static_cast<unsigned char>(
                static_cast<float>((color >> 8) & 0xff) * brightness);
            pixels[destination + 3] = static_cast<unsigned char>(alpha);
        }
    }

    const unsigned int texture_id = MikuPan_GPUCreateTextureRGBA8(
        output_width,
        output_height,
        pixels.data(),
        output_width * 4,
        mode == MikuPanTm2TextureMode::Standard ? 1 : 0,
        0);
    if (texture_id != 0)
    {
        texture_dimensions = {output_width, output_height};
    }
    return texture_id;
}

unsigned int LoadMikuPanSpriteTexture(int sprite_index,
                                      Rml::Vector2i& texture_dimensions)
{
#ifdef BUILD_EU_VERSION
    static constexpr uint32_t package_address_offset = 0x1d54030u;
#else
    static constexpr uint32_t package_address_offset = 0x1d59630u;
#endif
    static constexpr uint32_t ps2_memory_size = 32u * 1024u * 1024u;
    static constexpr uint32_t package_limit =
        ps2_memory_size - package_address_offset;

    if (sprite_index < 0 || sprite_index > PSP_END)
    {
        return 0;
    }

    const int64_t package_address =
        MikuPan_GetHostAddress(static_cast<int>(package_address_offset));
    if (package_address <= 0)
    {
        return 0;
    }

    const auto* package = reinterpret_cast<const uint32_t*>(package_address);
    const int texture_count = static_cast<int>(package[0]);
    if (texture_count <= 0 || texture_count > 256)
    {
        return 0;
    }

    const SPRT_DAT& sprite = spr_dat[sprite_index];
    TIM2_PICTUREHEADER* matched_picture = nullptr;

    for (int pass = 0; pass < 2 && matched_picture == nullptr; pass++)
    {
        for (int texture_index = 0; texture_index < texture_count; texture_index++)
        {
            const uint32_t offset = package[4 + texture_index];
            if (offset < 16 || offset >= package_limit)
            {
                continue;
            }

            void* tim2 = reinterpret_cast<void*>(package_address + offset);
            if (Tim2CheckFileHeaer(tim2) == 0)
            {
                continue;
            }

            const auto* header = reinterpret_cast<const TIM2_FILEHEADER*>(tim2);
            const int picture_count = std::clamp(static_cast<int>(header->Pictures),
                                                 0,
                                                 64);
            for (int picture_index = 0; picture_index < picture_count; picture_index++)
            {
                TIM2_PICTUREHEADER* picture =
                    Tim2GetPictureHeader(tim2, picture_index);
                if (picture == nullptr || picture->TotalSize == 0
                    || picture->TotalSize > package_limit - offset
                    || picture->ImageWidth == 0 || picture->ImageHeight == 0
                    || picture->ImageWidth > 4096 || picture->ImageHeight > 4096
                    || static_cast<int>(sprite.u) + static_cast<int>(sprite.w)
                           > picture->ImageWidth
                    || static_cast<int>(sprite.v) + static_cast<int>(sprite.h)
                           > picture->ImageHeight)
                {
                    continue;
                }

                const uint64_t picture_tex0 = picture->GsTex0;
                const uint64_t sprite_tex0 = sprite.tex0;
                bool matches = picture_tex0 == sprite_tex0;
                if (pass != 0)
                {
                    static constexpr uint64_t address_fields =
                        0x3fffull | (0x3fffull << 37u);
                    matches = (picture_tex0 & ~address_fields)
                              == (sprite_tex0 & ~address_fields);
                }

                if (matches)
                {
                    matched_picture = picture;
                    break;
                }
            }

            if (matched_picture != nullptr)
            {
                break;
            }
        }
    }

    if (matched_picture == nullptr || sprite.w == 0 || sprite.h == 0)
    {
        return 0;
    }

    const int width = sprite.w;
    const int height = sprite.h;
    const int clut = static_cast<int>((sprite.tex0 >> 56u) & 0x1fu);
    std::vector<unsigned char> pixels(
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const u_int color = Tim2GetTextureColor(matched_picture,
                                                    0,
                                                    clut,
                                                    sprite.u + x,
                                                    sprite.v + y);
            const size_t destination =
                static_cast<size_t>((y * width + x) * 4);
            pixels[destination + 0] = static_cast<unsigned char>(color & 0xff);
            pixels[destination + 1] =
                static_cast<unsigned char>((color >> 16) & 0xff);
            pixels[destination + 2] =
                static_cast<unsigned char>((color >> 8) & 0xff);
            pixels[destination + 3] = static_cast<unsigned char>(
                std::min(255u, ((color >> 24) & 0xffu) * 2u));
        }
    }

    const unsigned int texture_id = MikuPan_GPUCreateTextureRGBA8(width,
                                                                  height,
                                                                  pixels.data(),
                                                                  width * 4,
                                                                  0,
                                                                  0);
    if (texture_id != 0)
    {
        texture_dimensions = {width, height};
    }
    return texture_id;
}

class MikuPanRmlSystemInterface final : public Rml::SystemInterface
{
public:
    explicit MikuPanRmlSystemInterface(SDL_Window* in_window)
        : window(in_window)
    {
        cursor_default = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        cursor_pointer = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
        cursor_text = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    }

    ~MikuPanRmlSystemInterface() override
    {
        if (cursor_default != nullptr)
        {
            SDL_DestroyCursor(cursor_default);
        }
        if (cursor_pointer != nullptr)
        {
            SDL_DestroyCursor(cursor_pointer);
        }
        if (cursor_text != nullptr)
        {
            SDL_DestroyCursor(cursor_text);
        }
    }

    double GetElapsedTime() override
    {
        return static_cast<double>(SDL_GetTicks()) / 1000.0;
    }

    void SetMouseCursor(const Rml::String& cursor_name) override
    {
        SDL_Cursor* cursor = cursor_default;
        if (cursor_name == "pointer")
        {
            cursor = cursor_pointer;
        }
        else if (cursor_name == "text")
        {
            cursor = cursor_text;
        }

        if (cursor != nullptr)
        {
            SDL_SetCursor(cursor);
        }
    }

    void SetClipboardText(const Rml::String& text) override
    {
        SDL_SetClipboardText(text.c_str());
    }

    void GetClipboardText(Rml::String& text) override
    {
        char* clipboard = SDL_GetClipboardText();
        text = clipboard != nullptr ? clipboard : "";
        if (clipboard != nullptr)
        {
            SDL_free(clipboard);
        }
    }

    void ActivateKeyboard(Rml::Vector2f caret_position,
                          float line_height) override
    {
        if (window == nullptr)
        {
            return;
        }

        const SDL_Rect rect = {static_cast<int>(caret_position.x),
                               static_cast<int>(caret_position.y),
                               1,
                               static_cast<int>(line_height)};
        SDL_SetTextInputArea(window, &rect, 0);
        SDL_StartTextInput(window);
    }

    void DeactivateKeyboard() override
    {
        if (window != nullptr)
        {
            SDL_StopTextInput(window);
        }
    }

private:
    SDL_Window* window = nullptr;
    SDL_Cursor* cursor_default = nullptr;
    SDL_Cursor* cursor_pointer = nullptr;
    SDL_Cursor* cursor_text = nullptr;
};

class MikuPanRmlRenderInterface final : public Rml::RenderInterface
{
public:
    ~MikuPanRmlRenderInterface() override
    {
        if (white_texture_id != 0)
        {
            MikuPan_GPUReleaseTexture(white_texture_id);
            white_texture_id = 0;
        }
    }

    void SetWindowMetrics(int width, int height,
                          int viewport_x, int viewport_y,
                          int viewport_w, int viewport_h)
    {
        window_width = std::max(width, 1);
        window_height = std::max(height, 1);
        context_x = std::max(viewport_x, 0);
        context_y = std::max(viewport_y, 0);
        context_w = std::max(viewport_w, 1);
        context_h = std::max(viewport_h, 1);
        context_scale_x = 1.0f;
        context_scale_y = 1.0f;
    }

    int GetContextX(void) const { return context_x; }
    int GetContextY(void) const { return context_y; }
    int GetContextW(void) const { return context_w; }
    int GetContextH(void) const { return context_h; }

    Rml::CompiledGeometryHandle
    CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                    Rml::Span<const int> indices) override
    {
        auto* geometry = new MikuPanRmlGeometry();
        geometry->vertices.assign(vertices.begin(), vertices.end());
        geometry->indices.assign(indices.begin(), indices.end());
        return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override
    {
        delete reinterpret_cast<MikuPanRmlGeometry*>(geometry);
    }

    void SetTransform(const Rml::Matrix4f* transform) override
    {
        if (transform == nullptr)
        {
            transform_enabled = false;
            return;
        }

        current_transform = *transform;
        transform_enabled = true;
    }

    void RenderGeometry(Rml::CompiledGeometryHandle handle,
                        Rml::Vector2f translation,
                        Rml::TextureHandle texture) override
    {
        auto* geometry = reinterpret_cast<MikuPanRmlGeometry*>(handle);
        if (geometry == nullptr || geometry->indices.empty()
            || geometry->vertices.empty())
        {
            return;
        }

        unsigned int texture_id =
            static_cast<unsigned int>(static_cast<uintptr_t>(texture));
        if (texture_id == 0)
        {
            texture_id = EnsureWhiteTexture();
            if (texture_id == 0)
            {
                return;
            }
        }

        const size_t vertex_count = geometry->indices.size();

        scratch_vertices.clear();
        scratch_vertices.reserve(vertex_count * 12);

        for (int index : geometry->indices)
        {
            if (index < 0
                || static_cast<size_t>(index) >= geometry->vertices.size())
            {
                continue;
            }

            const Rml::Vertex& vertex = geometry->vertices[index];
            float x = vertex.position.x + translation.x;
            float y = vertex.position.y + translation.y;

            if (transform_enabled)
            {
                Rml::Vector4f transformed =
                    current_transform * Rml::Vector4f(x, y, 0.0f, 1.0f);
                const float w = transformed.w;
                if (std::fabs(w) > 0.000001f)
                {
                    x = transformed.x / w;
                    y = transformed.y / w;
                }
                else
                {
                    x = transformed.x;
                    y = transformed.y;
                }
            }

            x += static_cast<float>(context_x);
            y += static_cast<float>(context_y);

            const float ndc_x = (x / static_cast<float>(window_width)) * 2.0f
                                - 1.0f;
            const float ndc_y =
                1.0f - (y / static_cast<float>(window_height)) * 2.0f;

            float color[4];
            ConvertPremultipliedColor(vertex.colour, color);

            scratch_vertices.insert(
                scratch_vertices.end(),
                {vertex.tex_coord.x,
                 vertex.tex_coord.y,
                 0.0f,
                 0.0f,
                 color[0],
                 color[1],
                 color[2],
                 color[3],
                 ndc_x,
                 ndc_y,
                 0.0f,
                 1.0f});
        }

        if (scratch_vertices.empty())
        {
            return;
        }

        MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
        MikuPan_PipelineInfo* pipeline =
            MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
        if (pipeline == nullptr)
        {
            return;
        }

        MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_WINDOW, 0);
        MikuPan_GPUSetViewport(0, 0, window_width, window_height);
        ApplyScissor();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_SetRenderState2D();
        MikuPan_ActiveTextureCached(GL_TEXTURE0);
        MikuPan_BindTexture2DCached(texture_id);

        MikuPan_StreamUploadFull(
            GL_ARRAY_BUFFER,
            pipeline->buffers[0].id,
            static_cast<GLsizeiptr>(scratch_vertices.size() * sizeof(float)),
            scratch_vertices.data());
        MikuPan_TimedDrawArrays(GL_TRIANGLES,
                                0,
                                static_cast<GLsizei>(
                                    scratch_vertices.size() / 12));
        MikuPan_PerfDrawCall();
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
                                   const Rml::String& source) override
    {
        texture_dimensions = {0, 0};
        std::string normalized_source = source;
        const bool mirror_x = ExtractMirrorX(normalized_source);
        StripTextureVersion(normalized_source);

        int sprite_index = -1;
        if (ParseMikuPanSpriteSource(normalized_source, sprite_index))
        {
            const unsigned int texture_id = LoadMikuPanSpriteTexture(
                sprite_index,
                texture_dimensions);
            return static_cast<Rml::TextureHandle>(
                static_cast<uintptr_t>(texture_id));
        }

        int tm2_texture_index = -1;
        MikuPanTm2TextureMode tm2_texture_mode =
            MikuPanTm2TextureMode::Standard;
        if (ParseMikuPanTm2Source(normalized_source,
                                  tm2_texture_index,
                                  tm2_texture_mode))
        {
            const unsigned int texture_id = LoadMikuPanTm2Texture(
                tm2_texture_index,
                tm2_texture_mode,
                texture_dimensions);
            return static_cast<Rml::TextureHandle>(
                static_cast<uintptr_t>(texture_id));
        }

        SDL_Surface* surface = LoadPngSurface(normalized_source);
        if (surface == nullptr)
        {
            return 0;
        }

        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (converted != nullptr)
        {
            SDL_DestroySurface(surface);
            surface = converted;
        }

        if (mirror_x)
        {
            MirrorSurfaceX(surface);
        }

        const unsigned int texture_id =
            MikuPan_GPUCreateTextureFromSurface(surface);
        if (texture_id != 0)
        {
            texture_dimensions = {surface->w, surface->h};
        }
        SDL_DestroySurface(surface);

        return static_cast<Rml::TextureHandle>(
            static_cast<uintptr_t>(texture_id));
    }

    Rml::TextureHandle
    GenerateTexture(Rml::Span<const Rml::byte> source,
                    Rml::Vector2i source_dimensions) override
    {
        if (source.empty() || source_dimensions.x <= 0
            || source_dimensions.y <= 0)
        {
            return 0;
        }

        const int width = source_dimensions.x;
        const int height = source_dimensions.y;
        std::vector<unsigned char> pixels(source.begin(), source.end());
        ConvertPremultipliedTexture(pixels);

        const unsigned int texture_id =
            MikuPan_GPUCreateTextureRGBA8(width,
                                          height,
                                          pixels.data(),
                                          width * 4,
                                          0,
                                          0);
        return static_cast<Rml::TextureHandle>(
            static_cast<uintptr_t>(texture_id));
    }

    void ReleaseTexture(Rml::TextureHandle texture) override
    {
        const unsigned int texture_id =
            static_cast<unsigned int>(static_cast<uintptr_t>(texture));
        if (texture_id != 0)
        {
            MikuPan_GPUReleaseTexture(texture_id);
        }
    }

    void EnableScissorRegion(bool enable) override
    {
        scissor_enabled = enable;
    }

    void SetScissorRegion(Rml::Rectanglei region) override
    {
        scissor_x = region.Left();
        scissor_y = region.Top();
        scissor_w = region.Width();
        scissor_h = region.Height();
    }

private:
    static void ConvertPremultipliedColor(
        const Rml::ColourbPremultiplied& color,
        float out_color[4])
    {
        const float alpha = static_cast<float>(color.alpha) / 255.0f;
        out_color[3] = alpha;

        if (color.alpha == 0)
        {
            out_color[0] = 0.0f;
            out_color[1] = 0.0f;
            out_color[2] = 0.0f;
            return;
        }

        out_color[0] =
            static_cast<float>(color.red) / static_cast<float>(color.alpha);
        out_color[1] =
            static_cast<float>(color.green) / static_cast<float>(color.alpha);
        out_color[2] =
            static_cast<float>(color.blue) / static_cast<float>(color.alpha);
    }

    static void ConvertPremultipliedTexture(std::vector<unsigned char>& pixels)
    {
        for (size_t i = 0; i + 3 < pixels.size(); i += 4)
        {
            const unsigned int alpha = pixels[i + 3];
            if (alpha == 0)
            {
                pixels[i + 0] = 0;
                pixels[i + 1] = 0;
                pixels[i + 2] = 0;
                continue;
            }

            pixels[i + 0] = static_cast<unsigned char>(
                std::min(255u, (static_cast<unsigned int>(pixels[i + 0]) * 255u)
                                   / alpha));
            pixels[i + 1] = static_cast<unsigned char>(
                std::min(255u, (static_cast<unsigned int>(pixels[i + 1]) * 255u)
                                   / alpha));
            pixels[i + 2] = static_cast<unsigned char>(
                std::min(255u, (static_cast<unsigned int>(pixels[i + 2]) * 255u)
                                   / alpha));
        }
    }

    static bool ExtractMirrorX(std::string& source)
    {
        static constexpr const char* kMirrorQuery = "?mirror_x";
        static constexpr const char* kMirrorFragment = "#mirror_x";
        const size_t query = source.find(kMirrorQuery);
        if (query != std::string::npos)
        {
            source.erase(query, std::char_traits<char>::length(kMirrorQuery));
            return true;
        }

        const size_t fragment = source.find(kMirrorFragment);
        if (fragment != std::string::npos)
        {
            source.erase(fragment, std::char_traits<char>::length(kMirrorFragment));
            return true;
        }

        return false;
    }

    static void StripTextureVersion(std::string& source)
    {
        const size_t query = source.find("?v=");
        if (query != std::string::npos)
        {
            source.erase(query);
        }

        const size_t fragment = source.find("#v=");
        if (fragment != std::string::npos)
        {
            source.erase(fragment);
        }
    }

    static void MirrorSurfaceX(SDL_Surface* surface)
    {
        if (surface == nullptr || surface->pixels == nullptr
            || surface->w <= 1 || surface->h <= 0)
        {
            return;
        }

        auto* pixels = static_cast<unsigned char*>(surface->pixels);
        for (int y = 0; y < surface->h; y++)
        {
            unsigned char* row = pixels + y * surface->pitch;
            for (int x = 0; x < surface->w / 2; x++)
            {
                unsigned char* left = row + x * 4;
                unsigned char* right = row + (surface->w - 1 - x) * 4;
                for (int c = 0; c < 4; c++)
                {
                    std::swap(left[c], right[c]);
                }
            }
        }
    }

    static SDL_Surface* LoadPngSurface(const Rml::String& source)
    {
        if (source.empty())
        {
            return nullptr;
        }

        std::string normalized = source;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        if (normalized.rfind("file://", 0) == 0)
        {
            normalized.erase(0, 7);
        }
        else if (normalized.rfind("file:/", 0) == 0)
        {
            normalized.erase(0, 5);
        }

        if (normalized.size() >= 3
            && normalized[0] == '/'
            && normalized[2] == ':')
        {
            normalized.erase(0, 1);
        }

        SDL_Surface* surface = SDL_LoadPNG(normalized.c_str());
        if (surface != nullptr)
        {
            return surface;
        }

        std::array<std::string, 3> candidates = {
            normalized,
            std::string("resources/rml/") + normalized,
            std::string("resources/") + normalized,
        };

        for (const std::string& candidate : candidates)
        {
            char path[1024];
            if (!MikuPan_ResolveBasePath(candidate.c_str(), path, sizeof(path)))
            {
                continue;
            }

            surface = SDL_LoadPNG(path);
            if (surface != nullptr)
            {
                return surface;
            }
        }

        return nullptr;
    }

    unsigned int EnsureWhiteTexture(void)
    {
        if (white_texture_id != 0)
        {
            return white_texture_id;
        }

        static const unsigned char white_pixel[4] = {255, 255, 255, 255};
        white_texture_id = MikuPan_GPUCreateTextureRGBA8(1,
                                                         1,
                                                         white_pixel,
                                                         4,
                                                         0,
                                                         0);
        return white_texture_id;
    }

    void ApplyScissor(void) const
    {
        if (!scissor_enabled)
        {
            MikuPan_GPUDisableScissor();
            return;
        }

        const int x = std::max(context_x + scissor_x, context_x);
        const int y = std::max(context_y + scissor_y, context_y);
        const int w = std::max(std::min(scissor_w, context_x + context_w - x), 0);
        const int h = std::max(std::min(scissor_h, context_y + context_h - y), 0);
        MikuPan_GPUSetScissor(x, y, w, h);
    }

    int window_width = 1;
    int window_height = 1;
    int context_x = 0;
    int context_y = 0;
    int context_w = 1;
    int context_h = 1;
    float context_scale_x = 1.0f;
    float context_scale_y = 1.0f;
    bool transform_enabled = false;
    Rml::Matrix4f current_transform;
    bool scissor_enabled = false;
    int scissor_x = 0;
    int scissor_y = 0;
    int scissor_w = 1;
    int scissor_h = 1;
    std::vector<float> scratch_vertices;
    unsigned int white_texture_id = 0;
};


enum MikuPanRmlPadAction
{
    MIKUPAN_RML_PAD_UP = 0,
    MIKUPAN_RML_PAD_DOWN,
    MIKUPAN_RML_PAD_LEFT,
    MIKUPAN_RML_PAD_RIGHT,
    MIKUPAN_RML_PAD_CONFIRM,
    MIKUPAN_RML_PAD_ALT_CONFIRM,
    MIKUPAN_RML_PAD_CANCEL,
    MIKUPAN_RML_PAD_COUNT,
};

struct MikuPanRmlPadActionState
{
    bool down = false;
    uint64_t next_repeat_ticks = 0;
};

struct MikuPanRmlState
{
    SDL_Window* window = nullptr;
    std::unique_ptr<MikuPanRmlSystemInterface> system_interface;
    std::unique_ptr<MikuPanRmlRenderInterface> render_interface;
    Rml::Context* context = nullptr;
    std::vector<Rml::byte> font_data;
    std::array<MikuPanRmlPadActionState, MIKUPAN_RML_PAD_COUNT> pad_actions;
    bool pointer_input_active = true;
    bool initialized = false;
};

MikuPanRmlState g_rml;

void SetPointerInputActive(bool active)
{
    if (g_rml.pointer_input_active == active)
    {
        if (active)
        {
            SDL_ShowCursor();
        }
        return;
    }

    g_rml.pointer_input_active = active;
    if (active)
    {
        SDL_ShowCursor();
    }
    else
    {
        if (g_rml.context != nullptr)
        {
            g_rml.context->ProcessMouseLeave();
        }
        SDL_HideCursor();
    }
}

void UpdateContextDimensions(void)
{
    if (!g_rml.window || !g_rml.context || !g_rml.render_interface)
    {
        return;
    }

    int width = 1;
    int height = 1;
    SDL_GetWindowSizeInPixels(g_rml.window, &width, &height);
    width = std::max(width, 1);
    height = std::max(height, 1);

    int render_width = MikuPan_GetRenderResolutionWidth();
    int render_height = MikuPan_GetRenderResolutionHeight();
    render_width = std::max(render_width, 1);
    render_height = std::max(render_height, 1);

    const float target_aspect =
        static_cast<float>(render_width) / static_cast<float>(render_height);
    const float window_aspect =
        static_cast<float>(width) / static_cast<float>(height);

    int viewport_x = 0;
    int viewport_y = 0;
    int viewport_w = width;
    int viewport_h = height;
    if (window_aspect > target_aspect)
    {
        viewport_h = height;
        viewport_w = static_cast<int>(static_cast<float>(height) * target_aspect);
        viewport_x = (width - viewport_w) / 2;
        viewport_y = 0;
    }
    else
    {
        viewport_w = width;
        viewport_h = static_cast<int>(static_cast<float>(width) / target_aspect);
        viewport_x = 0;
        viewport_y = (height - viewport_h) / 2;
    }

    viewport_w = std::max(viewport_w, 1);
    viewport_h = std::max(viewport_h, 1);

    g_rml.render_interface->SetWindowMetrics(width,
                                             height,
                                             viewport_x,
                                             viewport_y,
                                             viewport_w,
                                             viewport_h);
    const Rml::Vector2i dimensions(viewport_w, viewport_h);
    if (g_rml.context->GetDimensions() != dimensions)
    {
        g_rml.context->SetDimensions(dimensions);
    }

    /*
     * Keep RML rendering at native viewport resolution for crisp text, but let
     * the RCSS use dp units as a 640x448 design space. This avoids the blurry
     * 640x448 render-then-scale path while still keeping the menu anchored to
     * the original PS2-style options background.
     */
    const float scale_x = static_cast<float>(viewport_w) / 640.0f;
    const float scale_y = static_cast<float>(viewport_h) / 448.0f;
    const float dp_ratio = std::max(0.75f, std::min(scale_x, scale_y));
    g_rml.context->SetDensityIndependentPixelRatio(dp_ratio);
}

bool LoadFont(void)
{
    char font_path[1024];
    if (!MikuPan_ResolveBasePath("resources/fonts/CenturyOldStyle.ttf",
                                 font_path,
                                 sizeof(font_path)))
    {
        return false;
    }

    const unsigned int font_size = MikuPan_GetFileSize(font_path);
    if (font_size == 0 || font_size > static_cast<unsigned int>(INT_MAX))
    {
        return false;
    }

    g_rml.font_data.resize(font_size);
    MikuPan_ReadFullFile(font_path,
                         reinterpret_cast<char*>(g_rml.font_data.data()));

    return Rml::LoadFontFace(
        Rml::Span<const Rml::byte>(g_rml.font_data.data(),
                                   g_rml.font_data.size()),
        "MikuPanRml",
        Rml::Style::FontStyle::Normal,
        Rml::Style::FontWeight::Normal);
}

int ConvertMouseButton(unsigned char button)
{
    switch (button)
    {
        case SDL_BUTTON_LEFT:
            return 0;
        case SDL_BUTTON_RIGHT:
            return 1;
        case SDL_BUTTON_MIDDLE:
            return 2;
        default:
            return -1;
    }
}

Rml::Vector2i ConvertMousePosition(float x, float y)
{
    if (g_rml.window == nullptr)
    {
        return {static_cast<int>(x), static_cast<int>(y)};
    }

    int window_w = 1;
    int window_h = 1;
    int pixel_w = 1;
    int pixel_h = 1;
    SDL_GetWindowSize(g_rml.window, &window_w, &window_h);
    SDL_GetWindowSizeInPixels(g_rml.window, &pixel_w, &pixel_h);

    const float scale_x =
        window_w > 0 ? static_cast<float>(pixel_w) / window_w : 1.0f;
    const float scale_y =
        window_h > 0 ? static_cast<float>(pixel_h) / window_h : 1.0f;

    const int pixel_x = static_cast<int>(x * scale_x);
    const int pixel_y = static_cast<int>(y * scale_y);

    if (g_rml.render_interface == nullptr)
    {
        return {pixel_x, pixel_y};
    }

    return {pixel_x - g_rml.render_interface->GetContextX(),
            pixel_y - g_rml.render_interface->GetContextY()};
}

int ConvertKeyModifiers(void)
{
    const SDL_Keymod mods = SDL_GetModState();
    int result = 0;
    if (mods & SDL_KMOD_CTRL)
    {
        result |= Rml::Input::KM_CTRL;
    }
    if (mods & SDL_KMOD_SHIFT)
    {
        result |= Rml::Input::KM_SHIFT;
    }
    if (mods & SDL_KMOD_ALT)
    {
        result |= Rml::Input::KM_ALT;
    }
    if (mods & SDL_KMOD_NUM)
    {
        result |= Rml::Input::KM_NUMLOCK;
    }
    if (mods & SDL_KMOD_CAPS)
    {
        result |= Rml::Input::KM_CAPSLOCK;
    }
    return result;
}

Rml::Input::KeyIdentifier ConvertKey(SDL_Keycode key)
{
    switch (key)
    {
        case SDLK_UNKNOWN:
            return Rml::Input::KI_UNKNOWN;
        case SDLK_ESCAPE:
            return Rml::Input::KI_ESCAPE;
        case SDLK_SPACE:
            return Rml::Input::KI_SPACE;
        case SDLK_0:
            return Rml::Input::KI_0;
        case SDLK_1:
            return Rml::Input::KI_1;
        case SDLK_2:
            return Rml::Input::KI_2;
        case SDLK_3:
            return Rml::Input::KI_3;
        case SDLK_4:
            return Rml::Input::KI_4;
        case SDLK_5:
            return Rml::Input::KI_5;
        case SDLK_6:
            return Rml::Input::KI_6;
        case SDLK_7:
            return Rml::Input::KI_7;
        case SDLK_8:
            return Rml::Input::KI_8;
        case SDLK_9:
            return Rml::Input::KI_9;
        case SDLK_A:
            return Rml::Input::KI_A;
        case SDLK_B:
            return Rml::Input::KI_B;
        case SDLK_C:
            return Rml::Input::KI_C;
        case SDLK_D:
            return Rml::Input::KI_D;
        case SDLK_E:
            return Rml::Input::KI_E;
        case SDLK_F:
            return Rml::Input::KI_F;
        case SDLK_G:
            return Rml::Input::KI_G;
        case SDLK_H:
            return Rml::Input::KI_H;
        case SDLK_I:
            return Rml::Input::KI_I;
        case SDLK_J:
            return Rml::Input::KI_J;
        case SDLK_K:
            return Rml::Input::KI_K;
        case SDLK_L:
            return Rml::Input::KI_L;
        case SDLK_M:
            return Rml::Input::KI_M;
        case SDLK_N:
            return Rml::Input::KI_N;
        case SDLK_O:
            return Rml::Input::KI_O;
        case SDLK_P:
            return Rml::Input::KI_P;
        case SDLK_Q:
            return Rml::Input::KI_Q;
        case SDLK_R:
            return Rml::Input::KI_R;
        case SDLK_S:
            return Rml::Input::KI_S;
        case SDLK_T:
            return Rml::Input::KI_T;
        case SDLK_U:
            return Rml::Input::KI_U;
        case SDLK_V:
            return Rml::Input::KI_V;
        case SDLK_W:
            return Rml::Input::KI_W;
        case SDLK_X:
            return Rml::Input::KI_X;
        case SDLK_Y:
            return Rml::Input::KI_Y;
        case SDLK_Z:
            return Rml::Input::KI_Z;
        case SDLK_SEMICOLON:
            return Rml::Input::KI_OEM_1;
        case SDLK_PLUS:
            return Rml::Input::KI_OEM_PLUS;
        case SDLK_COMMA:
            return Rml::Input::KI_OEM_COMMA;
        case SDLK_MINUS:
            return Rml::Input::KI_OEM_MINUS;
        case SDLK_PERIOD:
            return Rml::Input::KI_OEM_PERIOD;
        case SDLK_SLASH:
            return Rml::Input::KI_OEM_2;
        case SDLK_GRAVE:
            return Rml::Input::KI_OEM_3;
        case SDLK_LEFTBRACKET:
            return Rml::Input::KI_OEM_4;
        case SDLK_BACKSLASH:
            return Rml::Input::KI_OEM_5;
        case SDLK_RIGHTBRACKET:
            return Rml::Input::KI_OEM_6;
        case SDLK_DBLAPOSTROPHE:
            return Rml::Input::KI_OEM_7;
        case SDLK_KP_0:
            return Rml::Input::KI_NUMPAD0;
        case SDLK_KP_1:
            return Rml::Input::KI_NUMPAD1;
        case SDLK_KP_2:
            return Rml::Input::KI_NUMPAD2;
        case SDLK_KP_3:
            return Rml::Input::KI_NUMPAD3;
        case SDLK_KP_4:
            return Rml::Input::KI_NUMPAD4;
        case SDLK_KP_5:
            return Rml::Input::KI_NUMPAD5;
        case SDLK_KP_6:
            return Rml::Input::KI_NUMPAD6;
        case SDLK_KP_7:
            return Rml::Input::KI_NUMPAD7;
        case SDLK_KP_8:
            return Rml::Input::KI_NUMPAD8;
        case SDLK_KP_9:
            return Rml::Input::KI_NUMPAD9;
        case SDLK_KP_ENTER:
            return Rml::Input::KI_NUMPADENTER;
        case SDLK_KP_MULTIPLY:
            return Rml::Input::KI_MULTIPLY;
        case SDLK_KP_PLUS:
            return Rml::Input::KI_ADD;
        case SDLK_KP_MINUS:
            return Rml::Input::KI_SUBTRACT;
        case SDLK_KP_PERIOD:
            return Rml::Input::KI_DECIMAL;
        case SDLK_KP_DIVIDE:
            return Rml::Input::KI_DIVIDE;
        case SDLK_BACKSPACE:
            return Rml::Input::KI_BACK;
        case SDLK_TAB:
            return Rml::Input::KI_TAB;
        case SDLK_RETURN:
            return Rml::Input::KI_RETURN;
        case SDLK_PAUSE:
            return Rml::Input::KI_PAUSE;
        case SDLK_CAPSLOCK:
            return Rml::Input::KI_CAPITAL;
        case SDLK_PAGEUP:
            return Rml::Input::KI_PRIOR;
        case SDLK_PAGEDOWN:
            return Rml::Input::KI_NEXT;
        case SDLK_END:
            return Rml::Input::KI_END;
        case SDLK_HOME:
            return Rml::Input::KI_HOME;
        case SDLK_LEFT:
            return Rml::Input::KI_LEFT;
        case SDLK_UP:
            return Rml::Input::KI_UP;
        case SDLK_RIGHT:
            return Rml::Input::KI_RIGHT;
        case SDLK_DOWN:
            return Rml::Input::KI_DOWN;
        case SDLK_INSERT:
            return Rml::Input::KI_INSERT;
        case SDLK_DELETE:
            return Rml::Input::KI_DELETE;
        case SDLK_F1:
            return Rml::Input::KI_F1;
        case SDLK_F2:
            return Rml::Input::KI_F2;
        case SDLK_F3:
            return Rml::Input::KI_F3;
        case SDLK_F4:
            return Rml::Input::KI_F4;
        case SDLK_F5:
            return Rml::Input::KI_F5;
        case SDLK_F6:
            return Rml::Input::KI_F6;
        case SDLK_F7:
            return Rml::Input::KI_F7;
        case SDLK_F8:
            return Rml::Input::KI_F8;
        case SDLK_F9:
            return Rml::Input::KI_F9;
        case SDLK_F10:
            return Rml::Input::KI_F10;
        case SDLK_F11:
            return Rml::Input::KI_F11;
        case SDLK_F12:
            return Rml::Input::KI_F12;
        case SDLK_LSHIFT:
            return Rml::Input::KI_LSHIFT;
        case SDLK_RSHIFT:
            return Rml::Input::KI_RSHIFT;
        case SDLK_LCTRL:
            return Rml::Input::KI_LCONTROL;
        case SDLK_RCTRL:
            return Rml::Input::KI_RCONTROL;
        case SDLK_LALT:
            return Rml::Input::KI_LMENU;
        case SDLK_RALT:
            return Rml::Input::KI_RMENU;
        case SDLK_LGUI:
            return Rml::Input::KI_LMETA;
        case SDLK_RGUI:
            return Rml::Input::KI_RMETA;
        default:
            return Rml::Input::KI_UNKNOWN;
    }
}

void PulseKey(Rml::Input::KeyIdentifier key)
{
    if (g_rml.context == nullptr || key == Rml::Input::KI_UNKNOWN)
    {
        return;
    }

    g_rml.context->ProcessKeyDown(key, 0);
    g_rml.context->ProcessKeyUp(key, 0);
}

SDL_Gamepad* GetRmlNavigationGamepad(void)
{
    SDL_Gamepad* gamepad = MikuPan_GetController();
    if (gamepad != nullptr && SDL_GamepadConnected(gamepad))
    {
        return gamepad;
    }

    int gamepad_count = 0;
    SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&gamepad_count);
    if (gamepad_ids == nullptr)
    {
        return nullptr;
    }

    for (int i = 0; i < gamepad_count; i++)
    {
        gamepad = SDL_GetGamepadFromID(gamepad_ids[i]);
        if (gamepad != nullptr && SDL_GamepadConnected(gamepad))
        {
            SDL_free(gamepad_ids);
            return gamepad;
        }
    }

    SDL_free(gamepad_ids);
    return nullptr;
}

bool AxisNegative(SDL_Gamepad* gamepad, SDL_GamepadAxis axis)
{
    static constexpr int kDeadZone = 16000;
    return gamepad != nullptr && SDL_GetGamepadAxis(gamepad, axis) < -kDeadZone;
}

bool AxisPositive(SDL_Gamepad* gamepad, SDL_GamepadAxis axis)
{
    static constexpr int kDeadZone = 16000;
    return gamepad != nullptr && SDL_GetGamepadAxis(gamepad, axis) > kDeadZone;
}

Rml::Element* GetCurrentFocusElement(void)
{
    return g_rml.context != nullptr ? g_rml.context->GetFocusElement() : nullptr;
}

// These are the stock menu SFX IDs from iop/se/iopse.h.
static constexpr int kRmlSeCursor = 0; // SE_CSR0
static constexpr int kRmlSeClick = 1;  // SE_CLIC
static constexpr int kRmlSeCancel = 3; // SE_CANCEL

void PlayRmlUiSound(int se_no)
{
    (void) SeStartFix(se_no, 0, 0x1000, 0x1000, 1);
}

void PlayRmlCursorSoundIfMoved(Rml::Element* previous_focus)
{
    const bool focus_changed = previous_focus != GetCurrentFocusElement();
    const bool value_changed = MikuPan_RmlOptionsConsumeMoveSoundRequest() != 0;
    if (focus_changed || value_changed)
    {
        PlayRmlUiSound(kRmlSeCursor);
    }
}

void DispatchPadAction(MikuPanRmlPadAction action)
{
    SetPointerInputActive(false);

    if (MikuPan_RmlSavePointInputEnabled())
    {
        switch (action)
        {
            case MIKUPAN_RML_PAD_UP:
                MikuPan_RmlSavePointHandleVerticalInput(-1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_DOWN:
                MikuPan_RmlSavePointHandleVerticalInput(1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_LEFT:
                MikuPan_RmlSavePointHandleHorizontalInput(-1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_RIGHT:
                MikuPan_RmlSavePointHandleHorizontalInput(1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_CONFIRM:
            case MIKUPAN_RML_PAD_ALT_CONFIRM:
                MikuPan_RmlSavePointActivate();
                PlayRmlUiSound(kRmlSeClick);
                break;
            case MIKUPAN_RML_PAD_CANCEL:
                MikuPan_RmlSavePointHandleCancel();
                PlayRmlUiSound(kRmlSeCancel);
                break;
            default:
                break;
        }
        return;
    }

    if (MikuPan_RmlSaveLoadIsOpen())
    {
        switch (action)
        {
            case MIKUPAN_RML_PAD_UP:
                MikuPan_RmlSaveLoadHandleVerticalInput(-1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_DOWN:
                MikuPan_RmlSaveLoadHandleVerticalInput(1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_LEFT:
                MikuPan_RmlSaveLoadHandleHorizontalInput(-1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_RIGHT:
                MikuPan_RmlSaveLoadHandleHorizontalInput(1);
                PlayRmlUiSound(kRmlSeCursor);
                break;
            case MIKUPAN_RML_PAD_CONFIRM:
            case MIKUPAN_RML_PAD_ALT_CONFIRM:
                MikuPan_RmlSaveLoadActivate();
                PlayRmlUiSound(kRmlSeClick);
                break;
            case MIKUPAN_RML_PAD_CANCEL:
                MikuPan_RmlSaveLoadHandleCancel();
                PlayRmlUiSound(kRmlSeCancel);
                break;
            default:
                break;
        }
        return;
    }

    if (MikuPan_RmlOptionsInputBlocked())
    {
        return;
    }

    switch (action)
    {
        case MIKUPAN_RML_PAD_UP:
        {
            Rml::Element* previous_focus = GetCurrentFocusElement();
            const bool native_select_open = MikuPan_RmlOptionsAnySelectOpen() != 0;
            if (!MikuPan_RmlOptionsHandleVerticalInput(-1))
            {
                PulseKey(Rml::Input::KI_UP);
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            if (native_select_open)
            {
                PlayRmlUiSound(kRmlSeCursor);
            }
            else
            {
                PlayRmlCursorSoundIfMoved(previous_focus);
            }
            break;
        }
        case MIKUPAN_RML_PAD_DOWN:
        {
            Rml::Element* previous_focus = GetCurrentFocusElement();
            const bool native_select_open = MikuPan_RmlOptionsAnySelectOpen() != 0;
            if (!MikuPan_RmlOptionsHandleVerticalInput(1))
            {
                PulseKey(Rml::Input::KI_DOWN);
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            if (native_select_open)
            {
                PlayRmlUiSound(kRmlSeCursor);
            }
            else
            {
                PlayRmlCursorSoundIfMoved(previous_focus);
            }
            break;
        }
        case MIKUPAN_RML_PAD_LEFT:
        {
            Rml::Element* previous_focus = GetCurrentFocusElement();
            if (!MikuPan_RmlOptionsHandleHorizontalInput(-1))
            {
                PulseKey(Rml::Input::KI_LEFT);
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            PlayRmlCursorSoundIfMoved(previous_focus);
            break;
        }
        case MIKUPAN_RML_PAD_RIGHT:
        {
            Rml::Element* previous_focus = GetCurrentFocusElement();
            if (!MikuPan_RmlOptionsHandleHorizontalInput(1))
            {
                PulseKey(Rml::Input::KI_RIGHT);
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            PlayRmlCursorSoundIfMoved(previous_focus);
            break;
        }
        case MIKUPAN_RML_PAD_CONFIRM:
            if (!MikuPan_RmlOptionsActivateFocusedControl())
            {
                PulseKey(Rml::Input::KI_RETURN);
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            PlayRmlUiSound(kRmlSeClick);
            break;
        case MIKUPAN_RML_PAD_ALT_CONFIRM:
            if (!MikuPan_RmlOptionsActivateFocusedControl())
            {
                PulseKey(Rml::Input::KI_SPACE);
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            PlayRmlUiSound(kRmlSeClick);
            break;
        case MIKUPAN_RML_PAD_CANCEL:
            if (MikuPan_RmlOptionsAnySelectOpen())
            {
                PulseKey(Rml::Input::KI_ESCAPE);
            }
            else
            {
                MikuPan_RmlOptionsHandleCancel();
            }
            MikuPan_RmlOptionsEnsureFocusedControlVisible();
            PlayRmlUiSound(kRmlSeCancel);
            break;
        default:
            break;
    }
}

void UpdatePadAction(MikuPanRmlPadAction action,
                     bool down,
                     bool repeat_enabled,
                     uint64_t now)
{
    static constexpr uint64_t kInitialRepeatDelayMs = 320;
    static constexpr uint64_t kRepeatDelayMs = 90;

    MikuPanRmlPadActionState& state = g_rml.pad_actions[action];
    if (!down)
    {
        state.down = false;
        state.next_repeat_ticks = 0;
        return;
    }

    bool pulse = false;
    if (!state.down)
    {
        pulse = true;
        state.next_repeat_ticks = now + kInitialRepeatDelayMs;
    }
    else if (repeat_enabled && now >= state.next_repeat_ticks)
    {
        pulse = true;
        state.next_repeat_ticks = now + kRepeatDelayMs;
    }

    state.down = true;
    if (pulse)
    {
        DispatchPadAction(action);
    }
}

void ResetPadNavigationState(void)
{
    for (MikuPanRmlPadActionState& state : g_rml.pad_actions)
    {
        state = MikuPanRmlPadActionState();
    }
}

void UpdateGamepadNavigation(void)
{
    if (!MikuPan_RmlOptionsIsOpen() && !MikuPan_RmlSaveLoadIsOpen()
        && !MikuPan_RmlSavePointInputEnabled())
    {
        ResetPadNavigationState();
        return;
    }

    if (MikuPan_RmlOptionsIsOpen()
        && MikuPan_RmlOptionsBindingCaptureActive())
    {
        ResetPadNavigationState();
        return;
    }

    SDL_Gamepad* gamepad = GetRmlNavigationGamepad();
    if (gamepad == nullptr)
    {
        ResetPadNavigationState();
        return;
    }

    const bool up = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)
                    || AxisNegative(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    const bool down = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)
                      || AxisPositive(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    const bool left = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)
                      || AxisNegative(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    const bool right = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
                       || AxisPositive(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    const bool confirm = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH)
                         || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
    const bool alt_confirm = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST);
    const bool cancel = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST)
                        || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH)
                        || SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK);

    const uint64_t now = SDL_GetTicks();
    UpdatePadAction(MIKUPAN_RML_PAD_UP, up, true, now);
    UpdatePadAction(MIKUPAN_RML_PAD_DOWN, down, true, now);
    UpdatePadAction(MIKUPAN_RML_PAD_LEFT, left, true, now);
    UpdatePadAction(MIKUPAN_RML_PAD_RIGHT, right, true, now);
    UpdatePadAction(MIKUPAN_RML_PAD_CONFIRM, confirm, false, now);
    UpdatePadAction(MIKUPAN_RML_PAD_ALT_CONFIRM, alt_confirm, false, now);
    UpdatePadAction(MIKUPAN_RML_PAD_CANCEL, cancel, false, now);
}

} // namespace


extern "C" {

void MikuPan_RmlUiInit(SDL_Window* window)
{
    if (g_rml.initialized)
    {
        return;
    }

    g_rml.window = window;
    g_rml.system_interface = std::make_unique<MikuPanRmlSystemInterface>(window);
    g_rml.render_interface = std::make_unique<MikuPanRmlRenderInterface>();
    Rml::SetSystemInterface(g_rml.system_interface.get());
    Rml::SetRenderInterface(g_rml.render_interface.get());

    if (!Rml::Initialise())
    {
        g_rml.render_interface.reset();
        g_rml.window = nullptr;
        return;
    }

    int width = 1;
    int height = 1;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    width = std::max(width, 1);
    height = std::max(height, 1);
    int render_width = MikuPan_GetRenderResolutionWidth();
    int render_height = MikuPan_GetRenderResolutionHeight();
    render_width = std::max(render_width, 1);
    render_height = std::max(render_height, 1);

    const float target_aspect =
        static_cast<float>(render_width) / static_cast<float>(render_height);
    const float window_aspect =
        static_cast<float>(width) / static_cast<float>(height);
    int viewport_x = 0;
    int viewport_y = 0;
    int viewport_w = width;
    int viewport_h = height;
    if (window_aspect > target_aspect)
    {
        viewport_h = height;
        viewport_w = static_cast<int>(static_cast<float>(height) * target_aspect);
        viewport_x = (width - viewport_w) / 2;
    }
    else
    {
        viewport_w = width;
        viewport_h = static_cast<int>(static_cast<float>(width) / target_aspect);
        viewport_y = (height - viewport_h) / 2;
    }
    viewport_w = std::max(viewport_w, 1);
    viewport_h = std::max(viewport_h, 1);
    g_rml.render_interface->SetWindowMetrics(width,
                                             height,
                                             viewport_x,
                                             viewport_y,
                                             viewport_w,
                                             viewport_h);

    g_rml.context = Rml::CreateContext(
        "mikupan", Rml::Vector2i(viewport_w, viewport_h));
    if (g_rml.context != nullptr)
    {
        const float scale_x = static_cast<float>(viewport_w) / 640.0f;
        const float scale_y = static_cast<float>(viewport_h) / 448.0f;
        const float dp_ratio = std::max(0.75f, std::min(scale_x, scale_y));
        g_rml.context->SetDensityIndependentPixelRatio(dp_ratio);
    }
    if (g_rml.context == nullptr || !LoadFont()
        || !MikuPan_RmlOptionsInit(g_rml.context))
    {
        MikuPan_RmlSavePointPrepareShutdown();
        MikuPan_RmlSaveLoadPrepareShutdown();
        MikuPan_RmlOptionsPrepareShutdown();
        MikuPan_RmlTitlePrepareShutdown();
        Rml::Shutdown();
        MikuPan_RmlSavePointShutdown();
        MikuPan_RmlSaveLoadShutdown();
        MikuPan_RmlOptionsShutdown();
        MikuPan_RmlTitleShutdown();
        g_rml = MikuPanRmlState();
        return;
    }

    (void) MikuPan_RmlSaveLoadInit(g_rml.context);
    (void) MikuPan_RmlSavePointInit(g_rml.context);
    (void) MikuPan_RmlTitleInit(g_rml.context);

    g_rml.initialized = true;
}

void MikuPan_RmlUiStartFrame(void)
{
    if (!g_rml.initialized || g_rml.context == nullptr)
    {
        return;
    }

    UpdateContextDimensions();
    MikuPan_RmlOptionsStartFrame();
    MikuPan_RmlSaveLoadStartFrame();
    MikuPan_RmlSavePointStartFrame();
    MikuPan_RmlTitleStartFrame();
    UpdateGamepadNavigation();
    g_rml.context->Update();
}

void MikuPan_RmlUiRender(void)
{
    if (!g_rml.initialized || g_rml.context == nullptr)
    {
        return;
    }

    g_rml.context->Render();
    MikuPan_GPUDisableScissor();
}

void MikuPan_RmlUiProcessEvent(SDL_Event* event)
{
    if (!g_rml.initialized || g_rml.context == nullptr || event == nullptr)
    {
        return;
    }

    switch (event->type)
    {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            UpdateContextDimensions();
            break;
        case SDL_EVENT_MOUSE_MOTION:
        {
            SetPointerInputActive(true);
            const Rml::Vector2i mouse =
                ConvertMousePosition(event->motion.x, event->motion.y);
            g_rml.context->ProcessMouseMove(mouse.x,
                                            mouse.y,
                                            ConvertKeyModifiers());
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            if (!g_rml.pointer_input_active)
            {
                break;
            }

            const Rml::Vector2i mouse =
                ConvertMousePosition(event->button.x, event->button.y);
            g_rml.context->ProcessMouseMove(mouse.x,
                                            mouse.y,
                                            ConvertKeyModifiers());

            const int button = ConvertMouseButton(event->button.button);
            if (button >= 0)
            {
                g_rml.context->ProcessMouseButtonDown(button,
                                                      ConvertKeyModifiers());
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (!g_rml.pointer_input_active)
            {
                break;
            }

            const Rml::Vector2i mouse =
                ConvertMousePosition(event->button.x, event->button.y);
            g_rml.context->ProcessMouseMove(mouse.x,
                                            mouse.y,
                                            ConvertKeyModifiers());

            const int button = ConvertMouseButton(event->button.button);
            if (button >= 0)
            {
                g_rml.context->ProcessMouseButtonUp(button,
                                                    ConvertKeyModifiers());
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            if (!g_rml.pointer_input_active)
            {
                break;
            }

            float mouse_x = 0.0f;
            float mouse_y = 0.0f;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            const Rml::Vector2i mouse = ConvertMousePosition(mouse_x, mouse_y);
            g_rml.context->ProcessMouseMove(mouse.x,
                                            mouse.y,
                                            ConvertKeyModifiers());
            g_rml.context->ProcessMouseWheel(
                Rml::Vector2f(-event->wheel.x, -event->wheel.y),
                ConvertKeyModifiers());
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        {
            const Rml::Input::KeyIdentifier key = ConvertKey(event->key.key);
            if (MikuPan_RmlSavePointInputEnabled())
            {
                if (key == Rml::Input::KI_UP)
                {
                    MikuPan_RmlSavePointHandleVerticalInput(-1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_DOWN)
                {
                    MikuPan_RmlSavePointHandleVerticalInput(1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_LEFT)
                {
                    MikuPan_RmlSavePointHandleHorizontalInput(-1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_RIGHT)
                {
                    MikuPan_RmlSavePointHandleHorizontalInput(1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_RETURN
                         || key == Rml::Input::KI_SPACE)
                {
                    MikuPan_RmlSavePointActivate();
                    PlayRmlUiSound(kRmlSeClick);
                }
                else if (key == Rml::Input::KI_ESCAPE)
                {
                    MikuPan_RmlSavePointHandleCancel();
                    PlayRmlUiSound(kRmlSeCancel);
                }
                break;
            }
            if (MikuPan_RmlSaveLoadIsOpen())
            {
                if (key == Rml::Input::KI_UP)
                {
                    MikuPan_RmlSaveLoadHandleVerticalInput(-1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_DOWN)
                {
                    MikuPan_RmlSaveLoadHandleVerticalInput(1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_LEFT)
                {
                    MikuPan_RmlSaveLoadHandleHorizontalInput(-1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_RIGHT)
                {
                    MikuPan_RmlSaveLoadHandleHorizontalInput(1);
                    PlayRmlUiSound(kRmlSeCursor);
                }
                else if (key == Rml::Input::KI_RETURN
                         || key == Rml::Input::KI_SPACE)
                {
                    MikuPan_RmlSaveLoadActivate();
                    PlayRmlUiSound(kRmlSeClick);
                }
                else if (key == Rml::Input::KI_ESCAPE)
                {
                    MikuPan_RmlSaveLoadHandleCancel();
                    PlayRmlUiSound(kRmlSeCancel);
                }
                break;
            }
            if (MikuPan_RmlOptionsIsOpen())
            {
                if (MikuPan_RmlOptionsInputBlocked()
                    || MikuPan_RmlOptionsBindingCaptureActive())
                {
                    break;
                }
                if (key == Rml::Input::KI_UP
                    && MikuPan_RmlOptionsHandleVerticalInput(-1))
                {
                    break;
                }
                if (key == Rml::Input::KI_DOWN
                    && MikuPan_RmlOptionsHandleVerticalInput(1))
                {
                    break;
                }
                if (key == Rml::Input::KI_LEFT
                    && MikuPan_RmlOptionsHandleHorizontalInput(-1))
                {
                    break;
                }
                if (key == Rml::Input::KI_RIGHT
                    && MikuPan_RmlOptionsHandleHorizontalInput(1))
                {
                    break;
                }
                if (key == Rml::Input::KI_ESCAPE
                    && !MikuPan_RmlOptionsAnySelectOpen())
                {
                    MikuPan_RmlOptionsHandleCancel();
                    break;
                }
            }

            g_rml.context->ProcessKeyDown(key, ConvertKeyModifiers());
            break;
        }
        case SDL_EVENT_KEY_UP:
            if (MikuPan_RmlOptionsIsOpen()
                && (MikuPan_RmlOptionsInputBlocked()
                    || MikuPan_RmlOptionsBindingCaptureActive()))
            {
                break;
            }
            g_rml.context->ProcessKeyUp(ConvertKey(event->key.key),
                                        ConvertKeyModifiers());
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (MikuPan_RmlOptionsIsOpen()
                && (MikuPan_RmlOptionsInputBlocked()
                    || MikuPan_RmlOptionsBindingCaptureActive()))
            {
                break;
            }
            g_rml.context->ProcessTextInput(Rml::String(event->text.text));
            break;
        case SDL_EVENT_WINDOW_MOUSE_LEAVE:
            g_rml.context->ProcessMouseLeave();
            break;
        default:
            break;
    }
}

void MikuPan_RmlUiToggleHiWindow(void)
{
    MikuPan_RmlOptionsToggleDebug();
}

void MikuPan_RmlUiShutdown(void)
{
    if (!g_rml.initialized)
    {
        return;
    }

    MikuPan_RmlSavePointPrepareShutdown();
    MikuPan_RmlSaveLoadPrepareShutdown();
    MikuPan_RmlOptionsPrepareShutdown();
    MikuPan_RmlTitlePrepareShutdown();
    Rml::Shutdown();
    MikuPan_RmlSavePointShutdown();
    MikuPan_RmlSaveLoadShutdown();
    MikuPan_RmlOptionsShutdown();
    MikuPan_RmlTitleShutdown();
    g_rml = MikuPanRmlState();
}
}
