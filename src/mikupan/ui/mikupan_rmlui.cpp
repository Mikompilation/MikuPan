#include "mikupan/ui/mikupan_rmlui.h"

#include "glad/gl.h"
#include "mikupan/mikupan_file.h"
#include "mikupan/ui/mikupan_ui.h"

extern "C" {
#include "mikupan/rendering/mikupan_shader.h"
#include "mikupan/rendering/mikupan_pipeline.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_renderer_internal.h"
#include "mikupan/rendering/mikupan_gpu.h"
}

#include "SDL3/SDL.h"
#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/SystemInterface.h"
#include "RmlUi/Core/Elements/ElementFormControlInput.h"
#include "RmlUi/Core/Elements/ElementFormControlSelect.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct MikuPanRmlGeometry
{
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
};

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
    void SetWindowSize(int width, int height)
    {
        window_width = std::max(width, 1);
        window_height = std::max(height, 1);
    }

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

        const unsigned int texture_id =
            static_cast<unsigned int>(static_cast<uintptr_t>(texture));
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
            const float x = vertex.position.x + translation.x;
            const float y = vertex.position.y + translation.y;
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
        (void) source;
        texture_dimensions = {0, 0};
        return 0;
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

    void ApplyScissor(void) const
    {
        if (!scissor_enabled)
        {
            MikuPan_GPUDisableScissor();
            return;
        }

        const int x = std::max(scissor_x, 0);
        const int y = std::max(scissor_y, 0);
        const int w = std::max(std::min(scissor_w, window_width - x), 0);
        const int h = std::max(std::min(scissor_h, window_height - y), 0);
        MikuPan_GPUSetScissor(x, y, w, h);
    }

    int window_width = 1;
    int window_height = 1;
    bool scissor_enabled = false;
    int scissor_x = 0;
    int scissor_y = 0;
    int scissor_w = 1;
    int scissor_h = 1;
    std::vector<float> scratch_vertices;
};

class MikuPanSelectListener final : public Rml::EventListener
{
public:
    explicit MikuPanSelectListener(std::function<void(int)> callback)
        : on_change(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(
            event.GetCurrentElement());
        if (select == nullptr)
        {
            select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(
                event.GetTargetElement());
        }
        if (select == nullptr)
        {
            return;
        }

        on_change(select->GetSelection());
    }

private:
    std::function<void(int)> on_change;
};

class MikuPanInputListener final : public Rml::EventListener
{
public:
    explicit MikuPanInputListener(
        std::function<void(Rml::ElementFormControlInput*)> callback)
        : on_change(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(
            event.GetCurrentElement());
        if (input == nullptr)
        {
            input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(
                event.GetTargetElement());
        }
        if (input == nullptr)
        {
            return;
        }

        on_change(input);
    }

private:
    std::function<void(Rml::ElementFormControlInput*)> on_change;
};

class MikuPanButtonListener final : public Rml::EventListener
{
public:
    explicit MikuPanButtonListener(std::function<void()> callback)
        : on_click(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        (void) event;
        on_click();
    }

private:
    std::function<void()> on_click;
};

struct MikuPanRmlState
{
    SDL_Window* window = nullptr;
    std::unique_ptr<MikuPanRmlSystemInterface> system_interface;
    std::unique_ptr<MikuPanRmlRenderInterface> render_interface;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    Rml::Context* context = nullptr;
    Rml::ElementDocument* hi_document = nullptr;
    Rml::ElementFormControlSelect* window_mode_select = nullptr;
    Rml::ElementFormControlSelect* resolution_select = nullptr;
    Rml::ElementFormControlSelect* gpu_backend_select = nullptr;
    Rml::ElementFormControlSelect* msaa_select = nullptr;
    Rml::ElementFormControlSelect* shadow_resolution_select = nullptr;
    Rml::ElementFormControlSelect* lighting_mode_select = nullptr;
    Rml::ElementFormControlSelect* theme_select = nullptr;
    Rml::ElementFormControlSelect* font_select = nullptr;
    Rml::ElementFormControlInput* brightness_input = nullptr;
    Rml::Element* brightness_value = nullptr;
    Rml::ElementFormControlInput* gamma_input = nullptr;
    Rml::Element* gamma_value = nullptr;
    Rml::ElementFormControlInput* vsync_input = nullptr;
    Rml::Element* active_gpu_label = nullptr;
    Rml::Element* gpu_restart_note = nullptr;
    Rml::ElementFormControlInput* font_scale_input = nullptr;
    Rml::Element* font_scale_value = nullptr;
    Rml::ElementFormControlInput* crt_enabled_input = nullptr;
    std::vector<Rml::ElementFormControlInput*> crt_inputs;
    std::vector<Rml::Element*> crt_values;
    Rml::ElementFormControlInput* controller_remap_input = nullptr;
    Rml::ElementFormControlInput* data_folder_input = nullptr;
    Rml::Element* save_status = nullptr;
    std::vector<Rml::byte> font_data;
    bool initialized = false;
    bool hi_visible = false;
};

MikuPanRmlState g_rml;

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

    g_rml.render_interface->SetWindowSize(width, height);
    const Rml::Vector2i dimensions(width, height);
    if (g_rml.context->GetDimensions() != dimensions)
    {
        g_rml.context->SetDimensions(dimensions);
    }
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

std::string FormatFloat(float value, int precision)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return buffer;
}

float ParseFloat(const Rml::String& value, float fallback)
{
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    return end != value.c_str() ? parsed : fallback;
}

void SetElementText(Rml::Element* element, const std::string& text)
{
    if (element != nullptr)
    {
        element->SetInnerRML(text);
    }
}

void SetInputValue(Rml::ElementFormControlInput* input,
                   float value,
                   int precision)
{
    if (input != nullptr)
    {
        input->SetValue(FormatFloat(value, precision));
    }
}

void SetCheckbox(Rml::ElementFormControlInput* input, int enabled)
{
    if (input == nullptr)
    {
        return;
    }

    if (enabled)
    {
        input->SetAttribute("checked", "");
    }
    else
    {
        input->RemoveAttribute("checked");
    }
}

int IsCheckboxChecked(Rml::ElementFormControlInput* input)
{
    return input != nullptr && input->HasAttribute("checked");
}

void AddListener(Rml::Element* element,
                 Rml::EventId event_id,
                 std::unique_ptr<Rml::EventListener> listener)
{
    if (element == nullptr || listener == nullptr)
    {
        return;
    }

    element->AddEventListener(event_id, listener.get());
    g_rml.listeners.push_back(std::move(listener));
}

Rml::ElementFormControlSelect* GetSelect(const char* id)
{
    return rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(
        g_rml.hi_document->GetElementById(id));
}

Rml::ElementFormControlInput* GetInput(const char* id)
{
    return rmlui_dynamic_cast<Rml::ElementFormControlInput*>(
        g_rml.hi_document->GetElementById(id));
}

Rml::Element* GetElement(const char* id)
{
    return g_rml.hi_document->GetElementById(id);
}

void PopulateIndexedSelect(Rml::ElementFormControlSelect* select,
                           int count,
                           const char* (*get_label)(int),
                           int selected,
                           std::function<void(int)> on_change,
                           int (*is_enabled)(int) = nullptr)
{
    if (select == nullptr)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        const char* label = get_label != nullptr ? get_label(i) : nullptr;
        const bool enabled = is_enabled == nullptr || is_enabled(i);
        select->Add(label != nullptr ? label : "",
                    std::to_string(i),
                    -1,
                    enabled);
    }

    if (count > 0)
    {
        select->SetSelection(std::max(0, std::min(selected, count - 1)));
    }

    AddListener(select,
                Rml::EventId::Change,
                std::make_unique<MikuPanSelectListener>(std::move(on_change)));
}

void PopulateStaticSelect(Rml::ElementFormControlSelect* select,
                          const char* const* labels,
                          int count,
                          int selected,
                          std::function<void(int)> on_change)
{
    if (select == nullptr)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        select->Add(labels[i], std::to_string(i));
    }

    if (count > 0)
    {
        select->SetSelection(std::max(0, std::min(selected, count - 1)));
    }

    AddListener(select,
                Rml::EventId::Change,
                std::make_unique<MikuPanSelectListener>(std::move(on_change)));
}

void SyncRmlSettingsValues(void)
{
    if (g_rml.hi_document == nullptr)
    {
        return;
    }

    if (g_rml.window_mode_select != nullptr)
    {
        g_rml.window_mode_select->SetSelection(MikuPan_GetWindowMode());
    }
    if (g_rml.resolution_select != nullptr)
    {
        g_rml.resolution_select->SetSelection(
            MikuPan_GetSelectedRenderResolutionOption());
    }
    if (g_rml.gpu_backend_select != nullptr)
    {
        g_rml.gpu_backend_select->SetSelection(
            MikuPan_GetSelectedGpuDriverOption());
    }
    if (g_rml.msaa_select != nullptr)
    {
        g_rml.msaa_select->SetSelection(MikuPan_GetSelectedMSAAOption());
    }
    if (g_rml.shadow_resolution_select != nullptr)
    {
        g_rml.shadow_resolution_select->SetSelection(
            MikuPan_GetSelectedShadowResolutionOption());
    }
    if (g_rml.lighting_mode_select != nullptr)
    {
        g_rml.lighting_mode_select->SetSelection(MikuPan_GetLightingMode());
    }
    if (g_rml.theme_select != nullptr)
    {
        g_rml.theme_select->SetSelection(MikuPan_GetSelectedThemeOption());
    }
    if (g_rml.font_select != nullptr)
    {
        g_rml.font_select->SetSelection(MikuPan_GetSelectedFontOption());
    }

    SetInputValue(g_rml.brightness_input, MikuPan_GetBrightness(), 2);
    SetElementText(g_rml.brightness_value,
                   FormatFloat(MikuPan_GetBrightness(), 2));
    SetInputValue(g_rml.gamma_input, MikuPan_GetGamma(), 2);
    SetElementText(g_rml.gamma_value, FormatFloat(MikuPan_GetGamma(), 2));
    SetCheckbox(g_rml.vsync_input, MikuPan_IsVsync());
    SetElementText(g_rml.active_gpu_label,
                   std::string("Active: ")
                       + MikuPan_GetActiveGpuDriverLabel());
    SetElementText(g_rml.gpu_restart_note,
                   MikuPan_IsGpuDriverRestartPending()
                       ? "Save Configuration and restart to apply"
                       : "");
    SetInputValue(g_rml.font_scale_input, MikuPan_GetUiFontScale(), 2);
    SetElementText(g_rml.font_scale_value,
                   FormatFloat(MikuPan_GetUiFontScale(), 2) + "x");

    const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
    SetCheckbox(g_rml.crt_enabled_input, crt != nullptr && crt->enabled);
    for (size_t i = 0; i < g_rml.crt_inputs.size(); i++)
    {
        const int index = static_cast<int>(i);
        const float value = MikuPan_GetCrtFieldValue(index);
        const int precision = MikuPan_GetCrtFieldStep(index) < 0.01f ? 3 : 2;
        SetInputValue(g_rml.crt_inputs[i], value, precision);
        SetElementText(g_rml.crt_values[i], FormatFloat(value, precision));
    }

    SetCheckbox(g_rml.controller_remap_input,
                MikuPan_ShowControllerRemapWindow());

    if (g_rml.data_folder_input != nullptr
        && g_rml.context->GetFocusElement() != g_rml.data_folder_input)
    {
        g_rml.data_folder_input->SetValue(MikuPan_GetDataFolder());
    }

    SetElementText(g_rml.save_status, MikuPan_GetConfigSaveStatus());
}

bool LoadHiDocument(void)
{
    static constexpr const char* kHiDocument = R"(
<rml>
<head>
    <style>
        body {
            font-family: MikuPanRml;
            font-size: 15px;
            color: #d8d0c1;
        }
        #window {
            position: absolute;
            left: 26px;
            top: 48px;
            width: 590px;
            height: 700px;
            padding: 16px;
            background-color: #0a0908;
            border: 1px #6c6254;
            border-radius: 4dp;
        }
        #window:hover {
            background-color: #100f0d;
            border-color: #a19683;
        }
        #header {
            margin: 0 0 14px 0;
            padding: 0 0 10px 0;
            border-bottom: 1px #39342e;
            border-radius: 2dp;
        }
        #header:hover {
            background-color: #14110e;
        }
        #eyebrow {
            display: inline-block;
            margin: 0 0 7px 0;
            padding: 2px 7px;
            color: #b9b0a0;
            background-color: #1c1813;
            border: 1px #554b3e;
            border-radius: 3dp;
            font-size: 12px;
        }
        #eyebrow:hover {
            color: #eee5d3;
            background-color: #2a231b;
            border-color: #90826b;
        }
        h1 {
            margin: 0;
            font-size: 24px;
            color: #efe8d8;
        }
        h1:hover {
            color: #fff6e4;
        }
        p {
            margin: 3px 0 0 0;
            color: #8f877c;
            font-size: 13px;
        }
        p:hover {
            color: #c7bca9;
        }
        #settings-scroll {
            height: 620px;
            overflow-y: auto;
            padding-right: 6px;
        }
        .section {
            margin: 0 0 12px 0;
            padding: 12px;
            background-color: #11100e;
            border-radius: 3dp;
        }
        .section:hover {
            background-color: #171512;
        }
        h2 {
            margin: 0 0 10px 0;
            padding: 0 0 5px 0;
            color: #e6dcc8;
            border-bottom: 1px #3a332b;
            font-size: 16px;
        }
        h3 {
            margin: 12px 0 8px 0;
            color: #bfb39e;
            font-size: 13px;
        }
        .row {
            margin: 0 0 10px 0;
        }
        .row:hover {
            background-color: #1a1713;
        }
        .slider-head {
            margin: 0 0 5px 0;
        }
        .value {
            float: right;
            color: #a99779;
        }
        label {
            display: block;
            margin: 0 0 8px 0;
            color: #c7bfb1;
            font-size: 13px;
        }
        label:hover {
            color: #eee5d3;
        }
        .inline-label {
            display: inline-block;
            margin: 0 0 0 8px;
        }
        .note {
            margin: 6px 0 0 0;
            color: #8d8374;
            font-size: 12px;
        }
        #gpu-restart-note,
        #save-status {
            color: #c8ab77;
        }
        select {
            width: 100%;
            height: 34dp;
            padding: 0;
            margin: 0;
            font-family: MikuPanRml;
            font-size: 15px;
            color: #ded6c7;
            background-color: transparent;
            border-width: 0;
        }
        select selectvalue {
            width: auto;
            height: 25dp;
            margin-right: 30dp;
            padding: 9dp 10dp 0dp 10dp;
            color: #ded6c7;
            background-color: #0b0b0a;
            border: 1px #5b5246;
            border-radius: 2dp;
            white-space: nowrap;
        }
        select:hover selectvalue,
        select:focus-visible selectvalue,
        select selectvalue:checked {
            color: #fff3d4;
            background-color: #181510;
            border-color: #a39170;
        }
        select selectarrow {
            width: 30dp;
            height: 34dp;
            color: #b8aa90;
            background-color: #16130f;
            border: 1px #5b5246;
            border-radius: 2dp;
        }
        select:hover selectarrow,
        select:focus-visible selectarrow,
        select selectarrow:checked {
            color: #fff3d4;
            background-color: #2a2218;
            border-color: #a39170;
        }
        select selectbox {
            margin-top: 3px;
            padding: 2px;
            width: auto;
            max-height: 180px;
            background-color: #090807;
            border: 1px #7b6f5c;
            border-radius: 2dp;
        }
        select selectbox option {
            width: auto;
            min-height: 24px;
            padding: 6px 10px;
            font-family: MikuPanRml;
            font-size: 15px;
            color: #d8d0c0;
            background-color: #0d0c0a;
            white-space: nowrap;
        }
        select selectbox option:nth-child(even) {
            background-color: #12100d;
        }
        select selectbox option:hover {
            color: #fff5d7;
            background-color: #3a2d1e;
        }
        select selectbox option:checked {
            color: #f4dec0;
            background-color: #2b2117;
        }
        input.text {
            width: 100%;
            height: 29dp;
            padding: 5dp 8dp 0dp 8dp;
            color: #ded6c7;
            background-color: #0b0b0a;
            border: 1px #5b5246;
            border-radius: 2dp;
            font-family: MikuPanRml;
            font-size: 14px;
        }
        input.text:hover,
        input.text:focus-visible {
            color: #fff3d4;
            background-color: #181510;
            border-color: #a39170;
        }
        input.checkbox {
            width: 15dp;
            height: 15dp;
            background-color: #0b0b0a;
            border: 1px #6c6254;
            border-radius: 2dp;
        }
        input.checkbox:hover,
        input.checkbox:focus-visible {
            background-color: #211b14;
            border-color: #a39170;
        }
        input.checkbox:checked {
            background-color: #a39170;
            border-color: #e3d0ad;
        }
        input.range {
            width: 100%;
            height: 18dp;
        }
        input.range slidertrack {
            background-color: #080706;
            border: 1px #51483b;
        }
        input.range sliderprogress {
            background-color: #554530;
        }
        input.range sliderbar {
            width: 14dp;
            background-color: #9b8a70;
            border: 1px #d8c9a8;
            border-radius: 2dp;
        }
        input.range:hover sliderbar,
        input.range:focus-visible sliderbar {
            background-color: #d4bd92;
        }
        input.range sliderarrowdec,
        input.range sliderarrowinc {
            width: 12dp;
            height: 18dp;
            background-color: #15110d;
            border: 1px #51483b;
        }
        input.range sliderarrowdec:hover,
        input.range sliderarrowinc:hover {
            background-color: #2b2117;
            border-color: #a39170;
        }
        button {
            margin: 0 8px 0 0;
            padding: 7dp 12dp;
            color: #ded6c7;
            background-color: #16130f;
            border: 1px #625746;
            border-radius: 2dp;
            font-family: MikuPanRml;
            font-size: 13px;
            cursor: pointer;
        }
        button:hover,
        button:focus-visible {
            color: #fff3d4;
            background-color: #2a2218;
            border-color: #a39170;
        }
        scrollbarvertical {
            width: 10dp;
        }
        scrollbarvertical slidertrack {
            background-color: #080706;
        }
        scrollbarvertical sliderbar {
            background-color: #4c4236;
            border-radius: 2dp;
        }
        scrollbarvertical sliderbar:hover {
            background-color: #8e7c62;
        }
    </style>
</head>
<body>
    <div id="window">
        <div id="header">
            <div id="eyebrow">RmlUi</div>
            <h1>Settings</h1>
            <p>hi</p>
        </div>
        <div id="settings-scroll">
            <div class="section">
                <h2>Display</h2>
                <div class="row">
                    <label for="window-mode-select">Display Mode</label>
                    <select id="window-mode-select"></select>
                </div>
                <div class="row">
                    <label for="resolution-select">Resolution</label>
                    <select id="resolution-select"></select>
                </div>
                <div class="row">
                    <div class="slider-head">
                        <label for="brightness-input">Brightness <span id="brightness-value" class="value"></span></label>
                    </div>
                    <input type="range" id="brightness-input" min="0" max="2" step="0.01" />
                </div>
                <div class="row">
                    <div class="slider-head">
                        <label for="gamma-input">Gamma <span id="gamma-value" class="value"></span></label>
                    </div>
                    <input type="range" id="gamma-input" min="0.1" max="3" step="0.01" />
                </div>
                <div class="row">
                    <input type="checkbox" id="vsync-input" />
                    <label for="vsync-input" class="inline-label">VSync</label>
                </div>
            </div>
            <div class="section">
                <h2>Graphics</h2>
                <div class="row">
                    <label for="gpu-backend-select">GPU Backend</label>
                    <select id="gpu-backend-select"></select>
                    <p id="active-gpu-label" class="note"></p>
                    <p id="gpu-restart-note" class="note"></p>
                </div>
                <div class="row">
                    <label for="msaa-select">MSAA</label>
                    <select id="msaa-select"></select>
                </div>
                <div class="row">
                    <label for="shadow-resolution-select">Shadow Resolution</label>
                    <select id="shadow-resolution-select"></select>
                </div>
                <div class="row">
                    <label for="lighting-mode-select">Lighting Mode</label>
                    <select id="lighting-mode-select"></select>
                </div>
            </div>
            <div class="section">
                <h2>Theme</h2>
                <div class="row">
                    <label for="theme-select">Theme</label>
                    <select id="theme-select"></select>
                </div>
                <div class="row">
                    <label for="font-select">Font</label>
                    <select id="font-select"></select>
                </div>
                <div class="row">
                    <div class="slider-head">
                        <label for="font-scale-input">Font Size <span id="font-scale-value" class="value"></span></label>
                    </div>
                    <input type="range" id="font-scale-input" min="0.5" max="3" step="0.05" />
                </div>
            </div>
            <div class="section">
                <h2>CRT</h2>
                <div class="row">
                    <input type="checkbox" id="crt-enabled-input" />
                    <label for="crt-enabled-input" class="inline-label">Enabled</label>
                </div>
                <h3>Screen</h3>
                <div class="row"><label for="crt-0-input">Strength <span id="crt-0-value" class="value"></span></label><input type="range" id="crt-0-input" min="0" max="1" step="0.01" /></div>
                <div class="row"><label for="crt-1-input">Curvature <span id="crt-1-value" class="value"></span></label><input type="range" id="crt-1-input" min="0" max="0.3" step="0.001" /></div>
                <div class="row"><label for="crt-2-input">Overscan <span id="crt-2-value" class="value"></span></label><input type="range" id="crt-2-input" min="0" max="0.12" step="0.001" /></div>
                <h3>Scanlines</h3>
                <div class="row"><label for="crt-3-input">Scanline Strength <span id="crt-3-value" class="value"></span></label><input type="range" id="crt-3-input" min="0" max="1" step="0.01" /></div>
                <div class="row"><label for="crt-4-input">Scanline Scale <span id="crt-4-value" class="value"></span></label><input type="range" id="crt-4-input" min="0.25" max="3" step="0.01" /></div>
                <div class="row"><label for="crt-5-input">Scanline Thickness <span id="crt-5-value" class="value"></span></label><input type="range" id="crt-5-input" min="0.5" max="4" step="0.01" /></div>
                <h3>Mask</h3>
                <div class="row"><label for="crt-6-input">Mask Strength <span id="crt-6-value" class="value"></span></label><input type="range" id="crt-6-input" min="0" max="1" step="0.01" /></div>
                <div class="row"><label for="crt-7-input">Mask Scale <span id="crt-7-value" class="value"></span></label><input type="range" id="crt-7-input" min="0.5" max="4" step="0.01" /></div>
                <h3>Image</h3>
                <div class="row"><label for="crt-8-input">Vignette Strength <span id="crt-8-value" class="value"></span></label><input type="range" id="crt-8-input" min="0" max="1" step="0.01" /></div>
                <div class="row"><label for="crt-9-input">Vignette Size <span id="crt-9-value" class="value"></span></label><input type="range" id="crt-9-input" min="0.25" max="1.25" step="0.01" /></div>
                <div class="row"><label for="crt-10-input">Chroma Offset <span id="crt-10-value" class="value"></span></label><input type="range" id="crt-10-input" min="0" max="3" step="0.01" /></div>
                <div class="row"><label for="crt-11-input">Blend Strength <span id="crt-11-value" class="value"></span></label><input type="range" id="crt-11-input" min="0" max="1" step="0.01" /></div>
                <div class="row"><label for="crt-12-input">Blend Radius <span id="crt-12-value" class="value"></span></label><input type="range" id="crt-12-input" min="0" max="3" step="0.01" /></div>
                <div class="row"><label for="crt-13-input">Noise <span id="crt-13-value" class="value"></span></label><input type="range" id="crt-13-input" min="0" max="0.15" step="0.001" /></div>
                <div class="row"><label for="crt-14-input">Flicker <span id="crt-14-value" class="value"></span></label><input type="range" id="crt-14-input" min="0" max="0.10" step="0.001" /></div>
                <div class="row"><label for="crt-15-input">Glow <span id="crt-15-value" class="value"></span></label><input type="range" id="crt-15-input" min="0" max="0.50" step="0.01" /></div>
                <button id="reset-crt-button">Reset CRT</button>
            </div>
            <div class="section">
                <h2>Input</h2>
                <div class="row">
                    <input type="checkbox" id="controller-remap-input" />
                    <label for="controller-remap-input" class="inline-label">Controller / Joystick Mapping</label>
                </div>
                <button id="reset-bindings-button">Reset Bindings to Defaults</button>
            </div>
            <div class="section">
                <h2>Game Data Folder</h2>
                <div class="row">
                    <label for="data-folder-input">Folder</label>
                    <input type="text" id="data-folder-input" />
                </div>
                <button id="browse-folder-button">Browse...</button>
                <button id="save-config-button">Save Configuration</button>
                <p id="save-status" class="note"></p>
            </div>
        </div>
    </div>
</body>
</rml>
)";

    g_rml.hi_document =
        g_rml.context->LoadDocumentFromMemory(kHiDocument, "mikupan://hi");
    if (g_rml.hi_document == nullptr)
    {
        return false;
    }

    static constexpr const char* kWindowModeLabels[] = {
        "Windowed",
        "Fullscreen",
        "Borderless Fullscreen",
    };
    static constexpr const char* kLightingModeLabels[] = {
        "Pixel (Modern)",
        "Vertex (PS2)",
    };

    g_rml.window_mode_select = GetSelect("window-mode-select");
    PopulateStaticSelect(g_rml.window_mode_select,
                         kWindowModeLabels,
                         3,
                         MikuPan_GetWindowMode(),
                         [](int index) { MikuPan_SetWindowMode(index); });

    g_rml.resolution_select = GetSelect("resolution-select");
    if (g_rml.resolution_select != nullptr)
    {
        const int resolution_count = MikuPan_GetRenderResolutionOptionCount();
        if (resolution_count > 0)
        {
            PopulateIndexedSelect(
                g_rml.resolution_select,
                resolution_count,
                MikuPan_GetRenderResolutionOptionLabel,
                MikuPan_GetSelectedRenderResolutionOption(),
                [](int index) { MikuPan_SelectRenderResolutionOption(index); });
        }
        else
        {
            g_rml.resolution_select->Add("No display modes available",
                                         "-1",
                                         -1,
                                         false);
            g_rml.resolution_select->SetSelection(0);
        }
    }

    g_rml.brightness_input = GetInput("brightness-input");
    g_rml.brightness_value = GetElement("brightness-value");
    AddListener(
        g_rml.brightness_input,
        Rml::EventId::Change,
        std::make_unique<MikuPanInputListener>(
            [](Rml::ElementFormControlInput* input) {
                MikuPan_SetBrightness(
                    ParseFloat(input->GetValue(), MikuPan_GetBrightness()));
            }));

    g_rml.gamma_input = GetInput("gamma-input");
    g_rml.gamma_value = GetElement("gamma-value");
    AddListener(g_rml.gamma_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MikuPan_SetGamma(
                            ParseFloat(input->GetValue(), MikuPan_GetGamma()));
                    }));

    g_rml.vsync_input = GetInput("vsync-input");
    AddListener(g_rml.vsync_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MikuPan_SetVsync(IsCheckboxChecked(input));
                    }));

    g_rml.gpu_backend_select = GetSelect("gpu-backend-select");
    PopulateIndexedSelect(g_rml.gpu_backend_select,
                          MikuPan_GetGpuDriverOptionCount(),
                          MikuPan_GetGpuDriverOptionLabel,
                          MikuPan_GetSelectedGpuDriverOption(),
                          [](int index) { MikuPan_SelectGpuDriverOption(index); },
                          MikuPan_IsGpuDriverOptionSupported);
    g_rml.active_gpu_label = GetElement("active-gpu-label");
    g_rml.gpu_restart_note = GetElement("gpu-restart-note");

    g_rml.msaa_select = GetSelect("msaa-select");
    PopulateIndexedSelect(g_rml.msaa_select,
                          MikuPan_GetMSAAOptionCount(),
                          MikuPan_GetMSAAOptionLabel,
                          MikuPan_GetSelectedMSAAOption(),
                          [](int index) { MikuPan_SelectMSAAOption(index); });

    g_rml.shadow_resolution_select = GetSelect("shadow-resolution-select");
    PopulateIndexedSelect(g_rml.shadow_resolution_select,
                          MikuPan_GetShadowResolutionOptionCount(),
                          MikuPan_GetShadowResolutionOptionLabel,
                          MikuPan_GetSelectedShadowResolutionOption(),
                          [](int index) {
                              MikuPan_SelectShadowResolutionOption(index);
                          });

    g_rml.lighting_mode_select = GetSelect("lighting-mode-select");
    PopulateStaticSelect(g_rml.lighting_mode_select,
                         kLightingModeLabels,
                         2,
                         MikuPan_GetLightingMode(),
                         [](int index) { MikuPan_SetLightingMode(index); });

    g_rml.theme_select = GetSelect("theme-select");
    PopulateIndexedSelect(g_rml.theme_select,
                          MikuPan_GetThemeOptionCount(),
                          MikuPan_GetThemeOptionLabel,
                          MikuPan_GetSelectedThemeOption(),
                          [](int index) { MikuPan_SelectThemeOption(index); });

    g_rml.font_select = GetSelect("font-select");
    PopulateIndexedSelect(g_rml.font_select,
                          MikuPan_GetFontOptionCount(),
                          MikuPan_GetFontOptionLabel,
                          MikuPan_GetSelectedFontOption(),
                          [](int index) { MikuPan_SelectFontOption(index); });

    g_rml.font_scale_input = GetInput("font-scale-input");
    g_rml.font_scale_value = GetElement("font-scale-value");
    AddListener(
        g_rml.font_scale_input,
        Rml::EventId::Change,
        std::make_unique<MikuPanInputListener>(
            [](Rml::ElementFormControlInput* input) {
                MikuPan_SetUiFontScale(
                    ParseFloat(input->GetValue(), MikuPan_GetUiFontScale()));
            }));

    g_rml.crt_enabled_input = GetInput("crt-enabled-input");
    AddListener(g_rml.crt_enabled_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MikuPan_SetCrtEnabled(IsCheckboxChecked(input));
                    }));

    const int crt_count = MikuPan_GetCrtFieldCount();
    g_rml.crt_inputs.reserve(crt_count);
    g_rml.crt_values.reserve(crt_count);
    for (int i = 0; i < crt_count; i++)
    {
        const std::string input_id = "crt-" + std::to_string(i) + "-input";
        const std::string value_id = "crt-" + std::to_string(i) + "-value";
        auto* input = GetInput(input_id.c_str());
        g_rml.crt_inputs.push_back(input);
        g_rml.crt_values.push_back(GetElement(value_id.c_str()));
        AddListener(input,
                    Rml::EventId::Change,
                    std::make_unique<MikuPanInputListener>(
                        [i](Rml::ElementFormControlInput* control) {
                            MikuPan_SetCrtFieldValue(
                                i,
                                ParseFloat(control->GetValue(),
                                           MikuPan_GetCrtFieldValue(i)));
                        }));
    }

    AddListener(GetElement("reset-crt-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { MikuPan_ResetCrtSettings(); }));

    g_rml.controller_remap_input = GetInput("controller-remap-input");
    AddListener(g_rml.controller_remap_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MikuPan_SetControllerRemapWindowVisible(
                            IsCheckboxChecked(input));
                    }));
    AddListener(GetElement("reset-bindings-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { MikuPan_ResetControllerBindingsFromUi(); }));

    g_rml.data_folder_input = GetInput("data-folder-input");
    AddListener(g_rml.data_folder_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MikuPan_SetDataFolder(input->GetValue().c_str());
                    }));
    AddListener(GetElement("browse-folder-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { MikuPan_BrowseDataFolderFromUi(); }));
    AddListener(GetElement("save-config-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { MikuPan_SaveConfigurationFromUi(); }));
    g_rml.save_status = GetElement("save-status");

    SyncRmlSettingsValues();
    g_rml.hi_document->Hide();
    return true;
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

    return {static_cast<int>(x * scale_x), static_cast<int>(y * scale_y)};
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
    g_rml.render_interface->SetWindowSize(width, height);

    g_rml.context =
        Rml::CreateContext("mikupan", Rml::Vector2i(width, height));
    if (g_rml.context == nullptr || !LoadFont() || !LoadHiDocument())
    {
        Rml::Shutdown();
        g_rml = MikuPanRmlState();
        return;
    }

    g_rml.initialized = true;
}

void MikuPan_RmlUiStartFrame(void)
{
    if (!g_rml.initialized || g_rml.context == nullptr)
    {
        return;
    }

    UpdateContextDimensions();
    if (g_rml.hi_visible)
    {
        SyncRmlSettingsValues();
    }
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
            const Rml::Vector2i mouse =
                ConvertMousePosition(event->motion.x, event->motion.y);
            g_rml.context->ProcessMouseMove(mouse.x,
                                            mouse.y,
                                            ConvertKeyModifiers());
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
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
            const int button = ConvertMouseButton(event->button.button);
            if (button >= 0)
            {
                g_rml.context->ProcessMouseButtonUp(button,
                                                    ConvertKeyModifiers());
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            g_rml.context->ProcessMouseWheel(
                Rml::Vector2f(-event->wheel.x, -event->wheel.y),
                ConvertKeyModifiers());
            break;
        case SDL_EVENT_KEY_DOWN:
        {
            const Rml::Input::KeyIdentifier key = ConvertKey(event->key.key);
            g_rml.context->ProcessKeyDown(key, ConvertKeyModifiers());
            if (key == Rml::Input::KI_RETURN
                || key == Rml::Input::KI_NUMPADENTER)
            {
                g_rml.context->ProcessTextInput('\n');
            }
            break;
        }
        case SDL_EVENT_KEY_UP:
            g_rml.context->ProcessKeyUp(ConvertKey(event->key.key),
                                        ConvertKeyModifiers());
            break;
        case SDL_EVENT_TEXT_INPUT:
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
    if (!g_rml.initialized || g_rml.hi_document == nullptr)
    {
        return;
    }

    g_rml.hi_visible = !g_rml.hi_visible;
    if (g_rml.hi_visible)
    {
        g_rml.hi_document->Show();
    }
    else
    {
        g_rml.hi_document->Hide();
    }
}

void MikuPan_RmlUiShutdown(void)
{
    if (!g_rml.initialized)
    {
        return;
    }

    Rml::Shutdown();
    g_rml = MikuPanRmlState();
}
}
