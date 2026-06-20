#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "mikupan_framegraph.h"

#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/rendering/mikupan_profiler.h"

#include <string.h>

const int mikupan_frame_graph_window_style = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
                          | ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_NoHostExtendX;

const int mikupan_perf_window_flags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize;

const ImVec2 mikupan_style_origin = {0.0f, 0.0f};
const ImVec2 mikupan_plot_size = {0.0f, 60.0f};

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
    if (!igBeginTable(id, 3, mikupan_frame_graph_window_style,
                      mikupan_style_origin, 0.0f))
    {
        return 0;
    }

    igTableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
    igTableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
    igTableSetupColumn("share", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
    return 1;
}

void PerfRow(const char* label, float ms, float total)
{
    igTableNextRow(0, 0.0f);
    igTableSetColumnIndex(0);
    igTextUnformatted(label, NULL);
    igTableSetColumnIndex(1);
    igText("%6.2f", ms);
    igTableSetColumnIndex(2);

    if (total > 0.001f)
    {
        igText("%3.0f%%", 100.0f * ms / total);
    }
    else
    {
        igTextUnformatted("--", NULL);
    }
}

void FrameTimeGraph_Draw(FrameTimeGraph* g)
{
    if (g->count == 0)
    {
        igTextUnformatted("No frame data yet", NULL);
        return;
    }

    igBegin("Frame Time Graph", NULL, mikupan_perf_window_flags);

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

    igText("%.2f ms   %.0f FPS", total_latest, total_latest > 0.0f ? 1000.0f / total_latest : 0.0f);
    igSameLine(0.0f, -1.0f);

    if (gpu_latest < cpu_latest * 0.25f)
    {
        igTextColored((ImVec4) {1.0f, 0.6f, 0.3f, 1.0f}, "   - CPU submit");
    }
    else if (gpu_latest > cpu_latest)
    {
        igTextColored((ImVec4) {0.4f, 0.7f, 1.0f, 1.0f},"   - GPU/present wait");
    }
    else
    {
        igTextColored((ImVec4) {0.6f, 0.9f, 0.6f, 1.0f}, "   - balanced");
    }

    if (igBeginTable("##perf_summary", 4, mikupan_frame_graph_window_style,
                     mikupan_style_origin, 0.0f))
    {
        igTableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        igTableSetupColumn("now", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
        igTableSetupColumn("avg", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
        igTableSetupColumn("max", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);

#define MP_SUMMARY_ROW(name, now, avg, mx)                                     \
    igTableNextRow(0, 0.0f);                                                   \
    igTableSetColumnIndex(0);                                                  \
    igTextUnformatted(name, NULL);                                             \
    igTableSetColumnIndex(1);                                                  \
    igText("%6.2f", now);                                                      \
    igTableSetColumnIndex(2);                                                  \
    igText("%6.2f", avg);                                                      \
    igTableSetColumnIndex(3);                                                  \
    igText("%6.2f", mx)

        MP_SUMMARY_ROW("Frame", total_latest, total_avg, total_max);
        MP_SUMMARY_ROW("CPU", cpu_latest, cpu_avg, cpu_max);
        MP_SUMMARY_ROW("GPU wait", gpu_latest, gpu_avg, gpu_max);
#undef MP_SUMMARY_ROW
        igEndTable();
    }

    igTextDisabled("CPU = submit; GPU wait = SDL_WaitForGPUIdle after submit");

    if (mikupan_configuration.renderer.vsync)
    {
        igTextDisabled("VSync is on: wait can converge to the display interval.");
    }

    igSpacing();

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

    char total_overlay[64], cpu_overlay[64], gpu_overlay[64];
    snprintf(total_overlay, sizeof(total_overlay), "Total: %.2f ms", total_latest);
    snprintf(cpu_overlay, sizeof(cpu_overlay), "CPU:   %.2f ms", cpu_latest);
    snprintf(gpu_overlay, sizeof(gpu_overlay), "GPU wait: %.2f ms", gpu_latest);


    igPlotLines_FloatPtr("##frame_total", g->times, g->count, 0, total_overlay, 0.0f, scale, mikupan_plot_size, (int) sizeof(float));
    igPlotLines_FloatPtr("##frame_cpu", g->cpu, g->count, 0, cpu_overlay, 0.0f, scale, mikupan_plot_size, (int) sizeof(float));
    igPlotLines_FloatPtr("##frame_gpu", g->gpu, g->count, 0, gpu_overlay, 0.0f, scale, mikupan_plot_size, (int) sizeof(float));

    igSpacing();
    igSeparatorText("CPU breakdown (last frame)");

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
        igEndTable();
    }

    if (igTreeNode_Str("Mesh types"))
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
            igEndTable();
        }
        igTextDisabled("share is relative to Mesh render total");
        igTreePop();
    }

    if (igTreeNode_Str("Cross-cutting & state changes"))
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
            igEndTable();
        }
        igText("Texture L1: %d hits / %d misses  (miss -> XXH3 over GS memory)", l1_hits, l1_misses);
        igTextDisabled("subsets of the buckets above — not additional time");
        igTreePop();
    }

    igSpacing();
    igSeparatorText("GS texture traffic (last frame)");

    int ul_count = MikuPan_GsGetUploadCount();
    int ul_bytes = MikuPan_GsGetUploadBytes();
    int dl_count = MikuPan_GsGetDownloadCount();
    int dl_bytes = MikuPan_GsGetDownloadBytes();

    igText("Up %d calls / %.1f KB     Down %d calls / %.1f KB", ul_count, ul_bytes / 1024.0f, dl_count, dl_bytes / 1024.0f);

    igEnd();
}
