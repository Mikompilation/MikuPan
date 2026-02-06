#include "typedefs.h"
#include "mikupan_ui.h"
#include "glad/gl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "imgui_internal.h"
#include "imgui_toggle/imgui_toggle.h"
#include "main/glob.h"

extern "C"
{
#include "sce/libpad.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sglight.h"
}

#include <imgui.h>
#include <vector>
#include <numeric>
#include <cmath>

class FrameTimeGraph {
public:
    FrameTimeGraph(int max_samples = 300, float ms_scale = -1.0f)
        : max_samples_(std::max(8, max_samples)), ms_scale_(ms_scale)
    {
        times_.reserve(max_samples_);
    }

    void update(float dt_sec)
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

    void draw(const char* label = "Frame Time", ImVec2 size = ImVec2(0,0))
    {
        if (times_.empty())
        {
            ImGui::TextUnformatted("No frame data yet");
            return;
        }

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
    }

    void setMaxSamples(int max_samples)
    {
        max_samples_ = std::max(8, max_samples);
        times_.reserve(max_samples_);
        if ((int)times_.size() > max_samples_)
        {
            times_.erase(times_.begin(), times_.begin() + ((int)times_.size() - max_samples_));
        }
    }

    void setManualScaleMs(float ms)
    {
        ms_scale_ = ms;
    }

    void clear()
    {
        times_.clear();
    }

private:
    std::vector<float> times_;
    int max_samples_ = 300;
    float ms_scale_ = -1.0f; // negative means auto scale
    double sum_ms_ = 0.0; // reserved for future use
};

FrameTimeGraph g_frame_graph(600);

bool show_fps = true;
bool show_menu_bar = false;
bool show_frame_time_graph = false;
bool controller_config = false;
bool controller_rumble_test = false;
bool camera_debug = false;
int render_wireframe = 0;
int render_normals = 0;

void MikuPan_InitUi(SDL_Window *window, SDL_GLContext renderer)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplSDL3_InitForOpenGL(window, renderer);
    ImGui_ImplOpenGL3_Init("#version 130");
}

void MikuPan_RenderUi()
{
    ImGui::Render();
    glad_glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    //glad_glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void MikuPan_StartFrameUi()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void MikuPan_DrawUi()
{
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        show_menu_bar = !show_menu_bar;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2))
    {
        dbg_wrk.mode_on = !dbg_wrk.mode_on;
    }

    g_frame_graph.update(1000.0f / ImGui::GetIO().Framerate);

    if (show_menu_bar && ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::Toggle("FPS Counter", &show_fps, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Frame Time Graph", &show_frame_time_graph, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Ingame Debug Menu", (bool*)&dbg_wrk.mode_on, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Performance Info", (bool*)&dbg_wrk.oth_perf, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Packet Count", (bool*)&dbg_wrk.oth_pkt_num_sw, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Controller Config", &controller_config, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Clear Game", (bool*)&ingame_wrk.clear_count, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Camera", (bool*)&camera_debug, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Wireframe", (bool*)&render_wireframe, ImGuiToggleFlags_Animated);
            ImGui::Toggle("Normals", (bool*)&render_normals, ImGuiToggleFlags_Animated);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (controller_config)
    {
        ImGui::Begin("Controller");
        ImGui::Toggle("Rumble", &controller_rumble_test, ImGuiToggleFlags_Animated);
        ImGui::End();
    }

    if (controller_rumble_test)
    {
        //scePadSetActDirect(0, 0, NULL);
    }

    if (dbg_wrk.mode_on == 1)
    {
        gra2dDrawDbgMenu();
    }

    if (camera_debug)
    {
        //ImGui::InputFloat("Angle Snap", &snap.x);
    }

    if (show_frame_time_graph)
    {
        ImGui::Begin("Frame Time Graph");

        g_frame_graph.draw("Frame Time");
        ImGui::End();
    }

    if (show_fps)
    {
        SetString2(0x10, 0.0f, 420.0f, 1, 0x80, 0x80, 0x80, (char*)"FPS %d", (int)MikuPan_GetFrameRate());
    }
}

void MikuPan_ShutDownUi()
{
    ImGui_ImplOpenGL3_Shutdown();
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