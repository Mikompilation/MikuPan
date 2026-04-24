#include "typedefs.h"
#include "mikupan_ui.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "imgui_toggle/imgui_toggle.h"
#include "main/glob.h"
#include "mikupan/gs/mikupan_texture_manager.h"
#include <algorithm>
#include <string>

extern "C"
{
#include "sce/libpad.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sglight.h"
}

FrameTimeGraph g_frame_graph(600);
bool show_fps = false;
bool show_menu_bar = false;
bool show_frame_time_graph = false;
bool controller_config = false;
bool controller_rumble_test = false;
bool camera_debug = false;
int render_wireframe = 0;
int render_normals = 0;
int msaa_samples = 0;
int render_resolution_width = 640;
int render_resolution_height = 448;
bool show_texture_list = false;
bool show_bounding_boxes = false;
bool show_mesh_0x82 = true;
bool show_mesh_0x32 = true;
bool show_mesh_0x12 = true;
bool show_mesh_0x2 = true;

float light_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

FrameTimeGraph::FrameTimeGraph(int max_samples, float ms_scale) : max_samples_(std::max(8, max_samples)), ms_scale_(ms_scale)
{
    times_.reserve(max_samples_);
}

void FrameTimeGraph::update(float dt_sec)
{
    float ms = dt_sec;

    if ((int)times_.size() >= max_samples_)
    {
        times_.erase(times_.begin());
    }

    times_.push_back(ms);

    sum_ms_ += ms;
    if (times_.size() > 1) {
    }
}

void FrameTimeGraph::draw(const char *label, ImVec2 size)
{
    if (times_.empty())
    {
        ImGui::TextUnformatted("No frame data yet");
        return;
    }

    ImGui::Begin("Frame Time Graph");

    float sum = 0.0f;
    float minv = times_[0];
    float maxv = times_[0];

    for (float v : times_)
    {
        sum += v;
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }

    float avg = sum / (float)times_.size();
    float latest = times_.back();

    ImGui::Text("%s: %.1f ms (%.1f FPS)", label, latest, latest > 0.0f ? 1000.0f/latest : 0.0f);
    ImGui::Text("Avg: %.2f ms (%.1f FPS)  Min: %.2f ms  Max: %.2f ms  Samples: %zu", avg, avg > 0.0f ? 1000.0f/avg : 0.0f, minv, maxv, times_.size());

    float scale = ms_scale_ > 0.0f ? ms_scale_ : std::max(maxv, avg * 1.5f);
    if (scale < 1.0f) scale = 1.0f;

    ImGui::PlotLines("##plotlines", times_.data(), (int)times_.size(), 0, nullptr, 0.0f, scale, size);

    ImGui::Spacing();
    ImGui::Text("Frame time histogram (ms)");
    const int buckets = 16;
    std::vector<int> hist(buckets);
    float step = scale / (float)buckets;
    for (float v : times_)
    {
        int b = (int)std::floor(v / step);
        if (b < 0) b = 0;
        if (b >= buckets) b = buckets - 1;
        hist[b]++;
    }

    std::vector<float> histf(buckets);
    int maxcount = 1;
    for (int i = 0; i < buckets; ++i)
    {
        histf[i] = (float)hist[i];

        if (hist[i] > maxcount)
        {
            maxcount = hist[i];
        }
    }

    ImGui::PlotLines("##hist", histf.data(), buckets, 0, nullptr, 0.0f, (float)maxcount, ImVec2(0,60));

    ImGui::End();
}

void FrameTimeGraph::setMaxSamples(int max_samples)
{
    max_samples_ = std::max(8, max_samples);
    times_.reserve(max_samples_);
    if ((int)times_.size() > max_samples_)
    {
        times_.erase(times_.begin(), times_.begin() + ((int)times_.size() - max_samples_));
    }
}

void FrameTimeGraph::setManualScaleMs(float ms)
{
    ms_scale_ = ms;
}

void FrameTimeGraph::clear()
{
    times_.clear();
}

void MikuPan_InitUi(SDL_Window *window, SDL_GPUDevice *device)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL3_InitForSDLGPU(window);

    SDL_GPUTextureFormat swapchain_fmt =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device             = device;
    init_info.ColorTargetFormat  = swapchain_fmt;
    init_info.MSAASamples        = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);
}

void MikuPan_PrepareUi(SDL_GPUCommandBuffer *cmd)
{
    // Upload ImGui vertex/index data — must be called outside a render pass
    ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
}

void MikuPan_RenderUi(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass)
{
    // Record draw commands — must be called inside a render pass
    ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass);
}

void MikuPan_StartFrameUi()
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void MikuPan_DrawUi()
{
    g_frame_graph.update(1000.0f / ImGui::GetIO().Framerate);

    MikuPan_UiHandleShortcuts();

    MikuPan_UiMenuBar();

    if (controller_config)
    {
        ImGui::Begin("Controller");
        ImGui::Toggle("Rumble", &controller_rumble_test, ImGuiToggleFlags_Animated);
        ImGui::End();
    }

    if (dbg_wrk.mode_on == 1)
    {
        gra2dDrawDbgMenu();
    }

    if (show_frame_time_graph)
    {
        g_frame_graph.draw("Frame Time");
    }

    if (show_texture_list)
    {
        MikuPan_ShowTextureList();
    }

    if (show_fps)
    {
        SetString2(0x10, 0.0f, 420.0f, 1, 0x80, 0x80, 0x80, (char*)"FPS %d", (int)MikuPan_GetFrameRate());
    }

    ImGui::Render();
}

void MikuPan_ShutDownUi()
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void MikuPan_ProcessEventUi(SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

float MikuPan_GetFrameRate()
{
    return ImGui::GetIO().Framerate;
}

int MikuPan_IsWireframeRendering()
{
    return render_wireframe;
}

int MikuPan_IsNormalsRendering()
{
    return render_normals;
}

void MikuPan_ShowTextureList()
{
    ImGui::Begin("GPU Texture Cache");

    ImGui::Text("Cached textures: %zu", mikupan_render_texture_atlas.size());
    ImGui::Separator();

    for (const auto& pair : mikupan_render_texture_atlas)
    {
        const MikuPan_TextureInfo *t = pair.second;
        char label[64];
        SDL_snprintf(label, sizeof(label), "hash %016llx",
                     (unsigned long long)t->hash);

        if (ImGui::CollapsingHeader(label))
        {
            ImGui::Text("  size: %d x %d", t->width, t->height);
            ImGui::Text("  ptr:  %p", (void *)t->texture);

            const float max_dim = 256.0f;
            float display_w = (float)t->width;
            float display_h = (float)t->height;
            float largest = display_w > display_h ? display_w : display_h;
            if (largest > max_dim)
            {
                float scale = max_dim / largest;
                display_w *= scale;
                display_h *= scale;
            }
            ImGui::Image((ImTextureID)(intptr_t)t->texture,
                         ImVec2(display_w, display_h));
        }
    }

    ImGui::End();
}

void MikuPan_UiHandleShortcuts()
{
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        show_menu_bar = !show_menu_bar;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2))
    {
        dbg_wrk.mode_on = !dbg_wrk.mode_on;
    }
}

void MikuPan_UiMenuBar()
{
    if (show_menu_bar && ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Debug"))
        {
            if (ImGui::BeginMenu("Rendering"))
            {
                //ImGui::Toggle("Camera", (bool*)&camera_debug, ImGuiToggleFlags_Animated);
                ImGui::Toggle("Wireframe", (bool*)&render_wireframe, ImGuiToggleFlags_Animated);
                ImGui::Toggle("Normals", (bool*)&render_normals, ImGuiToggleFlags_Animated);
                ImGui::Toggle("Textures", (bool*)&show_texture_list, ImGuiToggleFlags_Animated);
                ImGui::Toggle("BoundingBox", (bool*)&show_bounding_boxes, ImGuiToggleFlags_Animated);

                if (ImGui::BeginMenu("Meshes"))
                {
                    ImGui::Toggle("Mesh 0x82", (bool*)&show_mesh_0x82, ImGuiToggleFlags_Animated);
                    ImGui::Toggle("Mesh 0x32", (bool*)&show_mesh_0x32, ImGuiToggleFlags_Animated);
                    ImGui::Toggle("Mesh 0x12", (bool*)&show_mesh_0x12, ImGuiToggleFlags_Animated);
                    ImGui::Toggle("Mesh 0x2", (bool*)&show_mesh_0x2, ImGuiToggleFlags_Animated);
                    ImGui::EndMenu();
                }

                if (ImGui::Button("Clear Texture Cache"))
                {
                    MikuPan_RequestFlushTextureCache();
                }

                ImGui::SliderInt("MSAA", &msaa_samples, 0, 3, "");
                ImGui::SameLine();
                ImGui::Text("%dx", msaa_samples << 1);
                ImGui::SliderInt("Width",  &render_resolution_width,  640, 5120);
                ImGui::SliderInt("Height", &render_resolution_height,  224, 1440);
                ImGui::SliderFloat4("Light Color", light_color, 0.0f, 3.0f, "%.3f");

                ImGui::EndMenu();
            }

            ImGui::Toggle("Ingame Debug Menu", (bool*)&dbg_wrk.mode_on, ImGuiToggleFlags_Animated);
            //ImGui::Toggle("Controller Config", &controller_config, ImGuiToggleFlags_Animated);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Performance"))
        {
            ImGui::Toggle("FPS Counter", &show_fps, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Frame Time Graph", &show_frame_time_graph, ImGuiToggleFlags_Animated);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

int MikuPan_IsBoundingBoxRendering()
{
    return show_bounding_boxes;
}

int MikuPan_IsMesh0x82Rendering()
{
    return show_mesh_0x82;
}

int MikuPan_IsMesh0x32Rendering()
{
    return show_mesh_0x32;
}

int MikuPan_IsMesh0x12Rendering()
{
    return show_mesh_0x12;
}

int MikuPan_IsMesh0x2Rendering()
{
    return show_mesh_0x2;
}

float* MikuPan_GetLightColor()
{
    return light_color;
}

int MikuPan_GetRenderResolutionWidth()
{
    return render_resolution_width;
}

int MikuPan_GetRenderResolutionHeight()
{
    return render_resolution_height;
}

int MikuPan_GetMSAA()
{
    return msaa_samples<<1;
}