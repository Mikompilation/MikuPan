#include "typedefs.h"

#include "imgui.h"
#include "implot.h"
#include "mikupan_framegraph.h"

#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_meshcache.h"
#include "mikupan/rendering/mikupan_profiler.h"

#include <string.h>

const int mikupan_frame_graph_window_style = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
                          | ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_NoHostExtendX;

const int mikupan_perf_window_flags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize;

const ImVec2 mikupan_style_origin = {0.0f, 0.0f};
const ImVec2 mikupan_plot_size = {0.0f, 180.0f};

static ImPlotSpec MikuPan_FrameLineSpec(float r, float g, float b)
{
    ImPlotSpec spec;
    spec.LineColor = ImVec4(r, g, b, 1.0f);
    spec.LineWeight = 1.6f;
    return spec;
}

static void FrameTimeGraph_DrawPlot(FrameTimeGraph* g, float scale)
{
    const double x_max = g->count > 1 ? (double) (g->count - 1) : 1.0;
    const ImPlotFlags plot_flags =
        ImPlotFlags_NoTitle | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect;
    const ImPlotAxisFlags x_axis_flags =
        ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels
        | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoMenus
        | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_LockMax;
    const ImPlotAxisFlags y_axis_flags =
        ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_LockMin
        | ImPlotAxisFlags_LockMax;

    if (!ImPlot::BeginPlot("##frame_time_plot", mikupan_plot_size, plot_flags))
    {
        return;
    }

    ImPlot::SetupAxes(nullptr, "ms", x_axis_flags, y_axis_flags);
    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, (double) scale,
                            ImPlotCond_Always);
    ImPlot::SetupLegend(ImPlotLocation_NorthWest,
                        ImPlotLegendFlags_NoMenus);

    ImPlot::PlotLine("Total", g->times, g->count, 1.0, 0.0,
                     MikuPan_FrameLineSpec(0.82f, 0.88f, 0.98f));
    ImPlot::PlotLine("CPU", g->cpu, g->count, 1.0, 0.0,
                     MikuPan_FrameLineSpec(0.96f, 0.62f, 0.32f));
    ImPlot::PlotLine("GPU wait", g->gpu, g->count, 1.0, 0.0,
                     MikuPan_FrameLineSpec(0.38f, 0.74f, 0.98f));

    ImPlot::EndPlot();
}

static void FrameTimeGraph_DrawMeshCacheStats(void)
{
    unsigned long long mesh_cache_bytes = 0;
    unsigned int mesh_cache_buffers = 0;
    unsigned int mesh_cache_entries = 0;
    MikuPan_MeshCache_GetStats(&mesh_cache_bytes, &mesh_cache_buffers,
                               &mesh_cache_entries);

    ImGui::Text("Cache: %u entries, %u buffers, %.1f MiB",
                mesh_cache_entries, mesh_cache_buffers,
                (double) mesh_cache_bytes / (1024.0 * 1024.0));
    ImGui::Text("GPU buffers: %u live / %u max",
                MikuPan_GPUGetLiveBufferCount(),
                MikuPan_GPUGetMaxBufferCount());
}

void FrameTimeGraph_Update(FrameTimeGraph* g, float total_ms,
                                  float cpu_ms, float gpu_ms)
{
    if (g->count >= g->max_samples)
    {
        memmove(g->times, g->times + 1, (g->count - 1) * sizeof(float));
        memmove(g->cpu, g->cpu + 1, (g->count - 1) * sizeof(float));
        memmove(g->gpu, g->gpu + 1, (g->count - 1) * sizeof(float));
        g->count--;
    }
    g->times[g->count] = total_ms;
    g->cpu[g->count] = cpu_ms;
    g->gpu[g->count] = gpu_ms;
    g->count++;
}

int PerfTableBegin(const char* id)
{
    if (!ImGui::BeginTable(id, 3, mikupan_frame_graph_window_style,
                           mikupan_style_origin, 0.0f))
    {
        return 0;
    }

    ImGui::TableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
    ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
    ImGui::TableSetupColumn("share", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
    return 1;
}

void PerfRow(const char* label, float ms, float total)
{
    ImGui::TableNextRow(0, 0.0f);
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label, nullptr);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%6.2f", ms);
    ImGui::TableSetColumnIndex(2);

    if (total > 0.001f)
    {
        ImGui::Text("%3.0f%%", 100.0f * ms / total);
    }
    else
    {
        ImGui::TextUnformatted("--", nullptr);
    }
}

void FrameTimeGraph_Draw(FrameTimeGraph* g)
{
    if (g->count == 0)
    {
        ImGui::TextUnformatted("No frame data yet", nullptr);
        return;
    }

    ImGui::Begin("Frame Time Graph", nullptr, mikupan_perf_window_flags);

    float total_sum = 0.0f, total_max = g->times[0];
    float cpu_sum = 0.0f, cpu_max = g->cpu[0];
    float gpu_sum = 0.0f, gpu_max = g->gpu[0];

    for (int i = 0; i < g->count; i++)
    {
        total_sum += g->times[i];
        if (g->times[i] > total_max)
        {
            total_max = g->times[i];
        }
        cpu_sum += g->cpu[i];
        if (g->cpu[i] > cpu_max)
        {
            cpu_max = g->cpu[i];
        }
        gpu_sum += g->gpu[i];
        if (g->gpu[i] > gpu_max)
        {
            gpu_max = g->gpu[i];
        }
    }

    float total_avg = total_sum / (float) g->count;
    float cpu_avg = cpu_sum / (float) g->count;
    float gpu_avg = gpu_sum / (float) g->count;
    float total_latest = g->times[g->count - 1];
    float cpu_latest = g->cpu[g->count - 1];
    float gpu_latest = g->gpu[g->count - 1];
    const int gpu_wait_enabled = MikuPan_PerfIsGpuWaitEnabled();

    ImGui::Text("%.2f ms   %.0f FPS", total_latest, total_latest > 0.0f ? 1000.0f / total_latest : 0.0f);
    ImGui::SameLine(0.0f, -1.0f);

    if (!gpu_wait_enabled)
    {
        ImGui::TextColored(ImVec4{0.55f, 0.60f, 0.68f, 1.0f}, "   - GPU wait off");
    }
    else if (gpu_latest < cpu_latest * 0.25f)
    {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.3f, 1.0f}, "   - CPU submit");
    }
    else if (gpu_latest > cpu_latest)
    {
        ImGui::TextColored(ImVec4{0.4f, 0.7f, 1.0f, 1.0f},"   - GPU/present wait");
    }
    else
    {
        ImGui::TextColored(ImVec4{0.6f, 0.9f, 0.6f, 1.0f}, "   - balanced");
    }

    if (ImGui::BeginTable("##perf_summary", 4, mikupan_frame_graph_window_style,
                          mikupan_style_origin, 0.0f))
    {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        ImGui::TableSetupColumn("now", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
        ImGui::TableSetupColumn("avg", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
        ImGui::TableSetupColumn("max", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);

#define MP_SUMMARY_ROW(name, now, avg, mx)                                     \
    ImGui::TableNextRow(0, 0.0f);                                              \
    ImGui::TableSetColumnIndex(0);                                             \
    ImGui::TextUnformatted(name, nullptr);                                     \
    ImGui::TableSetColumnIndex(1);                                             \
    ImGui::Text("%6.2f", now);                                                 \
    ImGui::TableSetColumnIndex(2);                                             \
    ImGui::Text("%6.2f", avg);                                                 \
    ImGui::TableSetColumnIndex(3);                                             \
    ImGui::Text("%6.2f", mx)

        MP_SUMMARY_ROW("Frame", total_latest, total_avg, total_max);
        MP_SUMMARY_ROW("CPU", cpu_latest, cpu_avg, cpu_max);
        MP_SUMMARY_ROW(gpu_wait_enabled ? "GPU wait" : "GPU wait (off)",
                       gpu_latest, gpu_avg, gpu_max);
#undef MP_SUMMARY_ROW
        ImGui::EndTable();
    }

    if (gpu_wait_enabled)
    {
        ImGui::TextDisabled("CPU = submit; GPU wait = SDL_WaitForGPUIdle after submit");
    }
    else
    {
        ImGui::TextDisabled("CPU = submit; GPU wait measurement is off");
    }

    if (gpu_wait_enabled && mikupan_configuration.renderer.vsync)
    {
        ImGui::TextDisabled("VSync is on: wait can converge to the display interval.");
    }

    ImGui::Spacing();

    float scale = total_max;

    if (cpu_max > scale)
    {
        scale = cpu_max;
    }

    if (gpu_max > scale)
    {
        scale = gpu_max;
    }

    if (g->ms_scale > 0.0f)
    {
        scale = g->ms_scale;
    }

    if (scale < 16.7f)
    {
        scale = 16.7f;
    }

    FrameTimeGraph_DrawPlot(g, scale);

    ImGui::Spacing();
    ImGui::SeparatorText("CPU breakdown (last frame)");

    float perf_mesh = MikuPan_PerfGetSectionMs(MP_PERF_MESH_RENDER);
    float perf_sprite = MikuPan_PerfGetSectionMs(MP_PERF_SPRITE_RENDER);
    float perf_flush = MikuPan_PerfGetSectionMs(MP_PERF_BATCH_FLUSH);
    float perf_drawui = MikuPan_PerfGetSectionMs(MP_PERF_DRAWUI);
    float perf_render = MikuPan_PerfGetSectionMs(MP_PERF_RENDERUI);
    float perf_gs_up = MikuPan_GsGetUploadMs();
    float perf_gs_dl = MikuPan_GsGetDownloadMs();

    float perf_skin = MikuPan_PerfGetSectionMs(MP_PERF_SKIN_PREP);
    float perf_coord = MikuPan_PerfGetSectionMs(MP_PERF_COORD_CALC);
    float perf_light = MikuPan_PerfGetSectionMs(MP_PERF_LIGHT_SETUP);

    float perf_known = perf_mesh + perf_sprite + perf_flush + perf_drawui
                       + perf_render + perf_gs_up + perf_gs_dl + perf_skin
                       + perf_coord + perf_light;
    float perf_other = cpu_latest - perf_known;

    if (perf_other < 0.0f)
    {
        perf_other = 0.0f;
    }

    if (PerfTableBegin("##cpu_breakdown"))
    {
        PerfRow("Mesh render", perf_mesh, cpu_latest);
        PerfRow("Sprite / UI render", perf_sprite, cpu_latest);
        PerfRow("Batch flushes", perf_flush, cpu_latest);
        PerfRow("DrawUi (game)", perf_drawui, cpu_latest);
        PerfRow("RenderUi (ImGui)", perf_render, cpu_latest);
        PerfRow("GS uploads", perf_gs_up, cpu_latest);
        PerfRow("GS downloads", perf_gs_dl, cpu_latest);
        PerfRow("Skin prep (SetVUVN)", perf_skin, cpu_latest);
        PerfRow("Coord calc (SetLWS)", perf_coord, cpu_latest);
        PerfRow("Light setup", perf_light, cpu_latest);
        PerfRow("Other / unknown", perf_other, cpu_latest);
        ImGui::EndTable();
    }

    if (ImGui::TreeNode("Mesh types"))
    {
        if (PerfTableBegin("##mesh_types"))
        {
            PerfRow("0x2", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x2),
                    perf_mesh);
            PerfRow("0xA", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0xA),
                    perf_mesh);
            PerfRow("0x10", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x10),
                    perf_mesh);
            PerfRow("0x12", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x12),
                    perf_mesh);
            PerfRow("0x32", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x32),
                    perf_mesh);
            PerfRow("0x82", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x82),
                    perf_mesh);
            ImGui::EndTable();
        }
        ImGui::TextDisabled("share is relative to Mesh render total");
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Mesh cache");
    FrameTimeGraph_DrawMeshCacheStats();

    if (ImGui::TreeNode("Cross-cutting & state changes"))
    {
        float perf_draw = MikuPan_PerfGetSectionMs(MP_PERF_DRAW_SUBMIT);
        float perf_upload = MikuPan_PerfGetSectionMs(MP_PERF_BUFFER_UPLOAD);
        float perf_state = MikuPan_PerfGetSectionMs(MP_PERF_STATE_CHANGE);
        float perf_sc_shader = MikuPan_PerfGetSectionMs(MP_PERF_SC_SHADER);
        float perf_sc_texture = MikuPan_PerfGetSectionMs(MP_PERF_SC_TEXTURE);
        float perf_sc_rs3d = MikuPan_PerfGetSectionMs(MP_PERF_SC_RS3D);
        float perf_sc_vao = MikuPan_PerfGetSectionMs(MP_PERF_SC_VAO);
        float perf_tex_l1 = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L1_LOOKUP);
        float perf_tex_hash = MikuPan_PerfGetSectionMs(MP_PERF_TEX_HASH);
        float perf_tex_l2 = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L2_LOOKUP);
        float perf_tex_create = MikuPan_PerfGetSectionMs(MP_PERF_TEX_CREATE);
        float perf_tex_bind = MikuPan_PerfGetSectionMs(MP_PERF_TEX_BIND);
        int l1_hits = MikuPan_PerfGetTexL1Hits();
        int l1_misses = MikuPan_PerfGetTexL1Misses();

        if (PerfTableBegin("##cross_cutting"))
        {
            PerfRow("Draw commands submit (driver/call)", perf_draw, cpu_latest);
            PerfRow("Buffer uploads (map+memcpy)", perf_upload, cpu_latest);
            PerfRow("State changes (cached path)", perf_state, cpu_latest);
            PerfRow("    Shader (glUseProgram)", perf_sc_shader, cpu_latest);
            PerfRow("    Texture (lookup+bind)", perf_sc_texture, cpu_latest);
            PerfRow("        L1 lookup (tex0->info)", perf_tex_l1, cpu_latest);
            PerfRow("        Hash (XXH3/GS mem)", perf_tex_hash, cpu_latest);
            PerfRow("        L2 lookup (hash->info)", perf_tex_l2, cpu_latest);
            PerfRow("        Create (glTexImage+mip)", perf_tex_create,
                    cpu_latest);
            PerfRow("        Bind (glBindTexture)", perf_tex_bind, cpu_latest);
            PerfRow("    RenderState (depth/cull/blend)", perf_sc_rs3d,
                    cpu_latest);
            PerfRow("    VAO (glBindVertexArray)", perf_sc_vao, cpu_latest);
            ImGui::EndTable();
        }
        ImGui::Text("Texture L1: %d hits / %d misses  (miss -> XXH3 over GS memory)", l1_hits, l1_misses);
        ImGui::TextDisabled("subsets of the buckets above — not additional time");
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("GS texture traffic (last frame)");

    int ul_count = MikuPan_GsGetUploadCount();
    int ul_bytes = MikuPan_GsGetUploadBytes();
    int dl_count = MikuPan_GsGetDownloadCount();
    int dl_bytes = MikuPan_GsGetDownloadBytes();

    ImGui::Text("Up %d calls / %.1f KB     Down %d calls / %.1f KB", ul_count, ul_bytes / 1024.0f, dl_count, dl_bytes / 1024.0f);

    ImGui::End();
}
