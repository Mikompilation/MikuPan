#include "typedefs.h"
#include "mikupan_ui_debug.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "glad/gl.h"
#include "graphics/graph2d/effect.h"
#include "graphics/graph2d/g2d_debug.h"
#include "ingame/camera/camera.h"
#include "ingame/camera/camera_types.h"
#include "main/glob.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_basictypes.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_first_person.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_meshcache.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_shader.h"
#include "mikupan_framegraph.h"

#include <math.h>
#include <stdlib.h>

const int mikupan_no_navigation_window =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize
    | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

#define TEXTURE_LIST_MAX 4096

static MikuPan_TextureInfo* tex_list[TEXTURE_LIST_MAX];
static int tex_list_count = 0;
static char last_reload_error[1280] = {0};
static int show_texture_list = 0;
static int show_shader_reload = 0;
static int show_draw_inspector = 0;
static int show_effect_debug_window = 0;
static int show_camera_debug = 0;
static int show_photo_debug_window = 0;
static int show_shadow_debug_window = 0;
static int shadow_debug_auto_probe = 0;
static int shadow_debug_flip_y = 1;
static float shadow_debug_preview_size = 384.0f;
static float photo_debug_preview_size = 256.0f;
static int disable_gs_uploads = 0;
static int show_bounding_boxes = 0;
static int show_mesh_0x82 = 1;
static int show_mesh_0x32 = 1;
static int show_mesh_0x10 = 1;
static int show_mesh_0x12 = 1;
static int show_mesh_0x2 = 1;
static int disable_lighting = 0;
static int show_static_lighting = 0;
static float normal_length = 10.0f;
static int render_wireframe = 0;
static int render_normals = 0;
static int show_frame_time_graph = 0;
static FrameTimeGraph g_frame_graph = {.count = 0,
                                       .max_samples = 600,
                                       .ms_scale = -1.0f};

void CollectTexture(MikuPan_TextureInfo* tex, void* ud)
{
    int* count = (int*) ud;

    if (*count < TEXTURE_LIST_MAX)
    {
        tex_list[(*count)++] = tex;
    }
}

int CompareTextureById(const void* a, const void* b)
{
    const MikuPan_TextureInfo* ta = *(const MikuPan_TextureInfo**) a;
    const MikuPan_TextureInfo* tb = *(const MikuPan_TextureInfo**) b;
    return (int) ta->id - (int) tb->id;
}

const char* GlPrimName(GLenum mode)
{
    switch (mode)
    {
        case GL_POINTS:
            return "POINTS";
        case GL_LINES:
            return "LINES";
        case GL_LINE_LOOP:
            return "LINE_LOOP";
        case GL_LINE_STRIP:
            return "LINE_STRIP";
        case GL_TRIANGLES:
            return "TRIANGLES";
        case GL_TRIANGLE_STRIP:
            return "TRIANGLE_STRIP";
        case GL_TRIANGLE_FAN:
            return "TRIANGLE_FAN";
        default:
            return "?";
    }
}

void MikuPan_UiDrawCallInspector(void)
{
    if (!show_draw_inspector)
    {
        if (MikuPan_DrawCapEnabled())
        {
            MikuPan_DrawCapEnable(0);
        }

        return;
    }

    igSetNextWindowSize((ImVec2) {780.0f, 540.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Draw Call Inspector", (bool*) &show_draw_inspector, 0))
    {
        igEnd();
        return;
    }

    int enabled = MikuPan_DrawCapEnabled();
    if (igCheckbox("Capture", (bool*) &enabled))
    {
        MikuPan_DrawCapEnable(enabled);
    }

    igSameLine(0.0f, -1.0f);
    int isolate = MikuPan_DrawCapGetIsolate();
    if (isolate >= 0)
    {
        igTextColored((ImVec4) {1.0f, 0.7f, 0.2f, 1.0f}, "  ISOLATING #%d",
                      isolate);
        igSameLine(0.0f, -1.0f);
        if (igButton("Show All", (ImVec2) {0, 0}))
        {
            MikuPan_DrawCapSetIsolate(-1);
        }
    }
    else
    {
        igTextDisabled("  (click 'Solo' on a row to render only that draw)");
    }

    int count = MikuPan_DrawCapLastCount();
    igText("Last frame: %d draw calls captured", count);

    igSpacing();
    igSeparator();

    // Header row + scrollable child so long captures stay manageable.
    igText("%-4s %-6s %-16s %-26s %-7s %-7s %-9s %s", "#", "skip?", "primitive",
           "shader", "vao", "tex0", "ms", "");

    igBeginChild_Str("##draw_list", (ImVec2) {0.0f, 0.0f}, 0, 0);

    for (int i = 0; i < count; i++)
    {
        const MikuPan_DrawCapEntry* e = MikuPan_DrawCapLastEntryAt(i);

        if (e == NULL)
        {
            break;
        }

        igPushID_Int(i);

        ImVec4 col = e->skipped ? (ImVec4) {0.6f, 0.6f, 0.6f, 1.0f}
                                : (ImVec4) {0.9f, 0.9f, 0.9f, 1.0f};

        igTextColored(col, "%-4d %-6s %-16s %-26s %-7u %-7u %7.3f  count=%d%s",
                      i, e->skipped ? "skip" : "draw", GlPrimName(e->mode),
                      e->shader_idx >= 0 ? MikuPan_GetShaderName(e->shader_idx)
                                         : "<external>",
                      (unsigned) e->vao, (unsigned) e->texture0, e->ms,
                      e->count, e->is_elements ? " (elements)" : "");

        igSameLine(0.0f, -1.0f);

        if (isolate == i)
        {
            if (igButton("Unsolo", (ImVec2) {70.0f, 0.0f}))
            {
                MikuPan_DrawCapSetIsolate(-1);
            }
        }
        else
        {
            if (igButton("Solo", (ImVec2) {70.0f, 0.0f}))
            {
                MikuPan_DrawCapSetIsolate(i);
            }
        }

        igPopID();
    }

    igEndChild();
    igEnd();
}

const char* MikuPan_CameraDebugKindName(int kind)
{
    switch (kind)
    {
        case 0:
            return "Normal";
        case 1:
            return "Battle";
        case 2:
            return "Drama";
        case 3:
            return "Door";
        default:
            return "Unknown";
    }
}

void MikuPan_CameraDebugVec3(const char* label, const sceVu0FVECTOR v,
                                    ImVec4 color)
{
    igTextColored(color, "%-15s %8.1f %8.1f %8.1f", label, v[0], v[1], v[2]);
}

void MikuPan_UiCameraDebugWindow(void)
{
    if (!show_camera_debug)
    {
        return;
    }

    const CAMERA_DEBUG_PATH* path = CameraGetDebugPath();
    sceVu0FVECTOR view_vec = {camera.i[0] - camera.p[0],
                              camera.i[1] - camera.p[1],
                              camera.i[2] - camera.p[2], 0.0f};
    float view_dist =
        sqrtf(view_vec[0] * view_vec[0] + view_vec[1] * view_vec[1]
              + view_vec[2] * view_vec[2]);

    igSetNextWindowSize((ImVec2) {360.0f, 0.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Camera World Info", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        igEnd();
        return;
    }

    if (path->active)
    {
        igText("Map camera: %s  no=%u old=%u  type=%u",
               MikuPan_CameraDebugKindName(path->kind), path->no, path->no_old,
               path->type);
    }
    else
    {
        igText("Map camera: runtime/free camera");
    }

    igText("Distance camera -> interest: %.1f", view_dist);
    igSeparator();

    MikuPan_CameraDebugVec3("camera.p", camera.p,
                            (ImVec4) {1.0f, 0.78f, 0.10f, 1.0f});
    MikuPan_CameraDebugVec3("camera.i", camera.i,
                            (ImVec4) {0.15f, 1.0f, 0.35f, 1.0f});
    MikuPan_CameraDebugVec3("view line", view_vec,
                            (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f});

    if (path->active)
    {
        igSeparator();
        igTextColored((ImVec4) {1.0f, 0.42f, 0.08f, 1.0f},
                      "authored camera path points: %u",
                      path->camera_path_points);
        igTextColored((ImVec4) {0.05f, 0.80f, 1.0f, 1.0f},
                      "authored interest path points: %u",
                      path->interest_path_points);

        if (path->change || path->no != path->no_old)
        {
            igTextColored((ImVec4) {1.0f, 0.10f, 0.30f, 1.0f},
                          "transition target active");
        }
    }

    igEnd();
}

void MikuPan_UiShaderReloadWindow(void)
{
    if (!show_shader_reload)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {560.0f, 480.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Shader Reload", (bool*) &show_shader_reload, 0))
    {
        igEnd();
        return;
    }

    igTextDisabled(
        "Edits to .vert/.frag/.geom files take effect after Reload.");
    igTextDisabled(
        "Failed reloads keep the live program — no need to restart.");
    igSpacing();

    if (igButton("Reload All  (F5)", (ImVec2) {160.0f, 0.0f}))
    {
        last_reload_error[0] = '\0';
        MikuPan_ReloadAllShaders(last_reload_error,
                                 (int) sizeof(last_reload_error));
    }
    igSameLine(0.0f, -1.0f);
    igTextDisabled("9 programs");

    igSpacing();
    igSeparator();
    igSpacing();

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        igPushID_Int(i);

        if (igButton("Reload", (ImVec2) {80.0f, 0.0f}))
        {
            last_reload_error[0] = '\0';
            MikuPan_ReloadShader(i, last_reload_error,
                                 (int) sizeof(last_reload_error));
        }
        igSameLine(0.0f, -1.0f);
        igText("%-26s id=%u", MikuPan_GetShaderName(i),
               (unsigned) shader_list[i]);

        // Source paths underneath, indented — handy when there's any doubt
        // about which file an edit has to land in for a given shader.
        const char* vp = MikuPan_GetShaderVertSource(i);
        const char* gp = MikuPan_GetShaderGeomSource(i);
        const char* fp = MikuPan_GetShaderFragSource(i);
        if (vp)
        {
            igText("    vert: %s", vp);
        }
        if (gp)
        {
            igText("    geom: %s", gp);
        }
        if (fp)
        {
            igText("    frag: %s", fp);
        }

        igSpacing();
        igPopID();
    }

    if (last_reload_error[0] != '\0')
    {
        igSpacing();
        igSeparator();
        igTextColored((ImVec4) {1.0f, 0.4f, 0.4f, 1.0f}, "Last reload error:");
        igTextWrapped("%s", last_reload_error);
        if (igButton("Clear", (ImVec2) {0, 0}))
        {
            last_reload_error[0] = '\0';
        }
    }

    igEnd();
}

void MikuPan_UiShadowDebugWindow(void)
{
    if (!show_shadow_debug_window)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {560.0f, 680.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Shadow Debug", (bool*) &show_shadow_debug_window, 0))
    {
        igEnd();
        return;
    }

    int shadows_on = MikuPan_IsShadowEnabled();
    if (igCheckbox("Enable Shadows", (bool*) &shadows_on))
    {
        MikuPan_SetShadowEnabled(shadows_on);
    }

    int receiver_debug_view = MikuPan_IsShadowReceiverDebugViewEnabled();
    if (igCheckbox("Receiver Debug Colors", (bool*) &receiver_debug_view))
    {
        MikuPan_SetShadowReceiverDebugViewEnabled(receiver_debug_view);
    }

    int inspect = MikuPan_IsShadowInspectEnabled();
    if (igCheckbox("Inspect Caster (orbit camera)", (bool*) &inspect))
    {
        MikuPan_SetShadowInspectEnabled(inspect);
    }
    if (inspect)
    {
        float yaw = 0.0f, pitch = 0.0f;
        MikuPan_GetShadowInspectAngles(&yaw, &pitch);
        int changed = 0;
        changed |= igSliderFloat("Yaw", &yaw, -3.14159f, 3.14159f, "%.2f", 0);
        changed |= igSliderFloat("Pitch", &pitch, -1.55f, 1.55f, "%.2f", 0);
        if (changed)
        {
            MikuPan_SetShadowInspectAngles(yaw, pitch);
        }

        int wireframe = MikuPan_IsShadowInspectWireframe();
        if (igCheckbox("Wireframe", (bool*) &wireframe))
        {
            MikuPan_SetShadowInspectWireframe(wireframe);
        }

        igTextDisabled("Orbits the shadow camera; the preview shows the");
        igTextDisabled("caster model from this angle (cast shadow is");
        igTextDisabled("overridden while this is on).");
    }

    if (shadow_debug_auto_probe)
    {
        MikuPan_ShadowDebugProbeTexture();
    }

    const MikuPan_ShadowDebugInfo* shadow_debug = MikuPan_GetShadowDebugInfo();

    igText("FBO: %s  status: 0x%04x",
           shadow_debug->fbo_initialized
               ? (shadow_debug->fbo_complete ? "complete" : "incomplete")
               : "not created",
           shadow_debug->fbo_status);
    igText("Matrix: %s  texture: %u (%dx%d)",
           shadow_debug->matrix_valid ? "valid" : "missing",
           shadow_debug->texture_id, shadow_debug->texture_size,
           shadow_debug->texture_size);

    igSeparator();
    igText("Passes: caster %d  receiver %d", shadow_debug->caster_passes,
           shadow_debug->receiver_passes);
    igText("Draws:  caster %d (%d indices)  receiver %d (%d indices)",
           shadow_debug->caster_draws, shadow_debug->caster_indices,
           shadow_debug->receiver_draws, shadow_debug->receiver_indices);

    if (!shadow_debug->fbo_complete && shadow_debug->fbo_initialized)
    {
        igTextColored(
            (ImVec4) {1.0f, 0.4f, 0.3f, 1.0f},
            "Shadow target is incomplete; caster rendering cannot write.");
    }
    else if (shadow_debug->caster_passes == 0)
    {
        igTextColored((ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
                      "No caster pass this frame.");
    }
    else if (shadow_debug->caster_draws == 0)
    {
        igTextColored(
            (ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
            "Caster meshes were found, but none reached a GL shadow draw.");
    }
    else if (shadow_debug->receiver_passes != 0
             && shadow_debug->receiver_draws == 0)
    {
        igTextColored(
            (ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
            "Receiver traversal ran, but no receiver meshes were drawn.");
    }

    igSeparator();
    if (igButton("Probe Shadow Map", (ImVec2) {0.0f, 0.0f}))
    {
        MikuPan_ShadowDebugProbeTexture();
        shadow_debug = MikuPan_GetShadowDebugInfo();
    }
    igSameLine(0.0f, -1.0f);
    igCheckbox("Auto Probe", (bool*) &shadow_debug_auto_probe);

    if (shadow_debug->probe_valid)
    {
        igText("Map probe: nonzero %d / %d (%.2f%%), max %d, avg %.3f",
               shadow_debug->probe_nonzero_pixels,
               shadow_debug->texture_size * shadow_debug->texture_size,
               shadow_debug->probe_coverage * 100.0f,
               shadow_debug->probe_max_value, shadow_debug->probe_average);

        if (shadow_debug->caster_draws != 0
            && shadow_debug->probe_nonzero_pixels == 0)
        {
            igTextColored(
                (ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
                "Caster draws happened, but the shadow map is empty.");
        }
    }
    else
    {
        igTextDisabled("Map probe: not sampled yet");
    }

    igSeparator();
    igText("Shadow Map (R8 occlusion, white = caster)");
    igSliderFloat("Texture Preview Size", &shadow_debug_preview_size, 64.0f,
                  768.0f, "%.0f", 0);
    igCheckbox("Flip Y", (bool*) &shadow_debug_flip_y);

    if (shadow_debug->texture_id != 0)
    {
        ImVec2 uv0 =
            shadow_debug_flip_y ? (ImVec2) {0.0f, 1.0f} : (ImVec2) {0.0f, 0.0f};
        ImVec2 uv1 =
            shadow_debug_flip_y ? (ImVec2) {1.0f, 0.0f} : (ImVec2) {1.0f, 1.0f};

        // Draw on a mid-grey background with a visible frame so the 256x256
        // extent is obvious even when the map is empty (all-black). The R8
        // channel is swizzled to RGB at texture creation, so this shows the
        // occlusion silhouette as proper grayscale rather than dark red.
        ImVec2 img_min = igGetCursorScreenPos();
        ImVec2 img_size =
            (ImVec2) {shadow_debug_preview_size, shadow_debug_preview_size};

        igImageWithBg(
            (ImTextureRef_c) {
                ._TexID = (ImTextureID) (uintptr_t) MikuPan_GPUGetTextureHandle(
                    shadow_debug->texture_id)},
            img_size, uv0, uv1,
            (ImVec4) {0.15f, 0.15f, 0.18f,
                      1.0f}, /* bg behind transparent/black */
            (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f}); /* tint */

        ImDrawList* draw_list = igGetWindowDrawList();
        ImVec2 img_max =
            (ImVec2) {img_min.x + img_size.x, img_min.y + img_size.y};
        ImDrawList_AddRect(
            draw_list, img_min, img_max,
            igGetColorU32_Vec4((ImVec4) {0.55f, 0.85f, 1.0f, 0.9f}), 0.0f, 0,
            0);
    }
    else
    {
        igTextDisabled("Shadow texture has not been created yet.");
    }

    igEnd();
}

void MikuPan_UiPhotoDebugWindow(void)
{
    if (!show_photo_debug_window)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {460.0f, 560.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Photo Debug", (bool*) &show_photo_debug_window, 0))
    {
        igEnd();
        return;
    }

    int force_preview = MikuPan_IsPhotoDebugForceOpaquePreviewEnabled();
    if (igCheckbox("Force Opaque Preview Overlay", (bool*) &force_preview))
    {
        MikuPan_SetPhotoDebugForceOpaquePreviewEnabled(force_preview);
    }

    int force_negative = MikuPan_IsPhotoDebugForceNegativePreviewEnabled();
    if (igCheckbox("Force Negative Final Pass", (bool*) &force_negative))
    {
        MikuPan_SetPhotoDebugForceNegativePreviewEnabled(force_negative);
    }

    float force_negative_strength =
        MikuPan_GetPhotoDebugForceNegativePreviewStrength();
    if (igSliderFloat("Forced Negative Strength", &force_negative_strength,
                      0.0f, 1.0f, "%.2f", 0))
    {
        MikuPan_SetPhotoDebugForceNegativePreviewStrength(
            force_negative_strength);
    }

    int target_rect = MikuPan_IsPhotoDebugTargetRectEnabled();
    if (igCheckbox("Show Target Rect", (bool*) &target_rect))
    {
        MikuPan_SetPhotoDebugTargetRectEnabled(target_rect);
    }

    int negative_layer = MikuPan_IsPhotoDebugNegativeLayerEnabled();
    if (igCheckbox("Show Negative Debug Layer", (bool*) &negative_layer))
    {
        MikuPan_SetPhotoDebugNegativeLayerEnabled(negative_layer);
    }

    igSeparator();

    const MikuPan_PhotoDebugInfo* photo_debug = MikuPan_GetPhotoDebugInfo();

    igText("Texture: %s  id: %u  size: %dx%d",
           photo_debug->texture_valid ? "valid" : "missing",
           photo_debug->texture_id, photo_debug->texture_width,
           photo_debug->texture_height);
    igText("Texture updates: %d", photo_debug->texture_update_count);
    igText("Negative source: %s  id: %u  size: %dx%d",
           photo_debug->negative_source_texture_valid ? "valid" : "missing",
           photo_debug->negative_source_texture_id,
           photo_debug->negative_source_texture_width,
           photo_debug->negative_source_texture_height);
    igText("Negative source updates: %d",
           photo_debug->negative_source_texture_update_count);

    igSeparator();
    igText("Queued: %s  count: %d", photo_debug->queued ? "yes" : "no",
           photo_debug->queue_count);
    igText("Effect overlay active: %s  count: %d",
           photo_debug->effect_overlay_active ? "yes" : "no",
           photo_debug->effect_overlay_count);
    igText("Effect overlay drawn in game: %s  count: %d",
           photo_debug->effect_overlay_drawn_in_game ? "yes" : "no",
           photo_debug->effect_overlay_in_game_count);
    igText("Negative final-pass active: %s  count: %d",
           photo_debug->negative_overlay_active ? "yes" : "no",
           photo_debug->negative_overlay_count);
    igText("Legacy negative sprite drawn: %s  count: %d",
           photo_debug->negative_overlay_drawn_in_game ? "yes" : "no",
           photo_debug->negative_overlay_in_game_count);
    igText("Force negative preview: %s  strength: %.2f",
           photo_debug->force_negative_preview_enabled ? "yes" : "no",
           photo_debug->force_negative_preview_strength);
    igText("Negative rect: %d,%d %dx%d  strength: %.2f",
           photo_debug->negative_x, photo_debug->negative_y,
           photo_debug->negative_w, photo_debug->negative_h,
           photo_debug->negative_strength);
    igText("Queue rect: %d,%d %dx%d  alpha: %d", photo_debug->queue_x,
           photo_debug->queue_y, photo_debug->queue_w, photo_debug->queue_h,
           photo_debug->queue_alpha);

    igSeparator();
    igText("Last draw: %s  count: %d",
           photo_debug->last_draw_valid ? "yes" : "no",
           photo_debug->draw_count);
    igText("Draw rect: %d,%d %dx%d  alpha: %d", photo_debug->draw_x,
           photo_debug->draw_y, photo_debug->draw_w, photo_debug->draw_h,
           photo_debug->draw_alpha);
    igText("Target rect drawn this frame: %s",
           photo_debug->target_rect_drawn ? "yes" : "no");
    igText("Negative debug layer drawn this frame: %s  count: %d",
           photo_debug->negative_debug_layer_drawn ? "yes" : "no",
           photo_debug->negative_debug_layer_count);

    igSeparator();
    igTextDisabled("Expected photo rect is usually 128,80 384x256.");
    igTextDisabled("Cyan negative debug layer marks the outside region.");
    igTextDisabled("If this preview is correct but the overlay is wrong,");
    igTextDisabled("the failure is compositing/order/state, not capture.");

    igSeparator();
    igText("Captured Photo Texture");
    igSliderFloat("Preview Size", &photo_debug_preview_size, 64.0f, 512.0f,
                  "%.0f", 0);

    if (photo_debug->texture_id != 0)
    {
        float aspect = 1.0f;
        if (photo_debug->texture_width > 0 && photo_debug->texture_height > 0)
        {
            aspect = (float) photo_debug->texture_width
                     / (float) photo_debug->texture_height;
        }

        ImVec2 img_size = (ImVec2) {photo_debug_preview_size,
                                    photo_debug_preview_size / aspect};

        igImageWithBg(
            (ImTextureRef_c) {
                ._TexID = (ImTextureID) (uintptr_t) MikuPan_GPUGetTextureHandle(
                    photo_debug->texture_id)},
            img_size, (ImVec2) {0.0f, 0.0f}, (ImVec2) {1.0f, 1.0f},
            (ImVec4) {0.12f, 0.12f, 0.14f, 1.0f},
            (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f});
    }
    else
    {
        igTextDisabled("No photo preview texture has been uploaded yet.");
    }

    igEnd();
}

void MikuPan_UiShadowResolutionCombo(const char* label)
{
    static const int res_values[] = {128, 256, 512, 1024, 2048};
    static const char* res_labels[] = {"128", "256", "512", "1024", "2048"};
    const int count = (int) (sizeof(res_values) / sizeof(res_values[0]));

    int cur = MikuPan_GetShadowResolution();
    int idx = 1; /* default 256 */
    for (int i = 0; i < count; i++)
    {
        if (res_values[i] == cur)
        {
            idx = i;
            break;
        }
    }

    if (igCombo_Str_arr(label, &idx, res_labels, count, -1))
    {
        MikuPan_SetShadowResolution(res_values[idx]);
    }
}

int MikuPan_IsWireframeRendering(void)
{
    return render_wireframe;
}

int MikuPan_IsNormalsRendering(void)
{
    return render_normals;
}

static const char* MikuPanUIDebugBlendModeName(MikuPan_GPUBlendMode mode)
{
    switch (mode)
    {
    case MIKUPAN_GPU_BLEND_ADDITIVE:
        return "additive";
    case MIKUPAN_GPU_BLEND_SUBTRACTIVE:
        return "subtractive";
    case MIKUPAN_GPU_BLEND_SRC_TIMES_DST_ADD:
        return "src*dst+dst";
    case MIKUPAN_GPU_BLEND_NORMAL:
    default:
        return "normal";
    }
}

void MikuPan_ShowTextureList(void)
{
    const MikuPan_TextureInfo* screen_copy;

    tex_list_count = 0;
    MikuPan_ForEachTexture(CollectTexture, &tex_list_count);

    qsort(tex_list, tex_list_count, sizeof(MikuPan_TextureInfo*),
          CompareTextureById);

    igBegin("Texture List", NULL, 0);

    screen_copy = MikuPan_GetScreenCopyTextureInfo();
    if (screen_copy != NULL)
    {
        if (igCollapsingHeader_TreeNodeFlags(
                "Generated: distortion screen copy (0x1a40)",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            const MikuPan_ScreenCopyDebugInfo* debug =
                MikuPan_GetScreenCopyDebugInfo();
            float preview_w = (float) screen_copy->width;
            float preview_h = (float) screen_copy->height;

            if (preview_w > 512.0f)
            {
                preview_h *= 512.0f / preview_w;
                preview_w = 512.0f;
            }

            igText("%u: %d x %d", screen_copy->id, screen_copy->width,
                   screen_copy->height);
            if (debug != NULL)
            {
                const char* depth =
                    debug->depth_mode == MIKUPAN_DEPTH_ALWAYS   ? "always"
                    : debug->depth_mode == MIKUPAN_DEPTH_GEQUAL ? "gequal"
                                                                : "lequal";

                igText("last draw: %d vertices, depth %s, blend %s",
                       debug->vertex_count, depth,
                       MikuPanUIDebugBlendModeName(debug->blend_mode));
                igText("uv:  %.3f %.3f  ->  %.3f %.3f", debug->uv_min[0],
                       debug->uv_min[1], debug->uv_max[0], debug->uv_max[1]);
                igText("ndc: %.3f %.3f  ->  %.3f %.3f", debug->ndc_min[0],
                       debug->ndc_min[1], debug->ndc_max[0], debug->ndc_max[1]);
            }
            igImage((ImTextureRef_c) {._TexID = (ImTextureID) (uintptr_t)
                                          MikuPan_GPUGetTextureHandle(
                                              screen_copy->id)},
                    (ImVec2) {preview_w, preview_h}, (ImVec2) {0.0f, 0.0f},
                    (ImVec2) {1.0f, 1.0f});
        }
    }
    else
    {
        igText("Generated distortion screen copy: not created yet");
    }

    for (int i = 0; i < tex_list_count; i++)
    {
        char label[64];
        snprintf(label, sizeof(label), "Texture ID %u", tex_list[i]->id);

        if (igCollapsingHeader_TreeNodeFlags(label, 0))
        {
            igText("%u: %d x %d", tex_list[i]->id, tex_list[i]->width,
                   tex_list[i]->height);
            igImage((ImTextureRef_c) {._TexID = (ImTextureID) (uintptr_t)
                                          MikuPan_GPUGetTextureHandle(
                                              tex_list[i]->id)},
                    (ImVec2) {(float) tex_list[i]->width,
                              (float) tex_list[i]->height},
                    (ImVec2) {0.0f, 0.0f}, (ImVec2) {1.0f, 1.0f});
        }
    }

    igEnd();
}

float MikuPan_GetFrameRate(void)
{
    ImGuiIO* io = igGetIO_Nil();
    return io->Framerate;
}

void MikuPan_UiEffectDebugWindow(void)
{
    if (!show_effect_debug_window)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {720.0f, 560.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Effect Debug", (bool*) &show_effect_debug_window, 0))
    {
        igEnd();
        return;
    }

    if (igButton("Enable All", (ImVec2) {150.0f, 0.0f}))
    {
        MikuPan_EffectDebugSetAll(1);
    }

    igSameLine(0.0f, -1.0f);
    if (igButton("Clear All", (ImVec2) {90.0f, 0.0f}))
    {
        MikuPan_EffectDebugSetAll(0);
    }

    igSpacing();
    igTextDisabled("Rows list EFFECT_ID names and the render function/path used by the test toggle.");
    igTextDisabled("Unavailable rows need live enemy or scene-specific state, or have no renderer in this port.");
    igSeparator();

    const int table_flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
        | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollY;

    if (igBeginTable("##effect_debug_table", 4, table_flags,
                     (ImVec2) {0.0f, 0.0f}, 0.0f))
    {
        igTableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 48.0f, 0);
        igTableSetupColumn("Enum", ImGuiTableColumnFlags_WidthStretch, 0.35f,
                           0);
        igTableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch,
                           0.30f, 0);
        igTableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch, 0.35f,
                           0);
        igTableHeadersRow();

        const int count = MikuPan_EffectDebugCount();
        for (int id = 0; id < count; id++)
        {
            const int can_toggle = MikuPan_EffectDebugCanToggle(id);
            int enabled = MikuPan_EffectDebugEnabled(id);

            igPushID_Int(id);
            igTableNextRow(0, 0.0f);

            igTableNextColumn();
            if (can_toggle)
            {
                if (igCheckbox("##enabled", (bool*) &enabled))
                {
                    MikuPan_EffectDebugSetEnabled(id, enabled);
                }
            }
            else
            {
                igTextDisabled("-");
            }

            igTableNextColumn();
            if (can_toggle)
            {
                igText("%02d  %s", id, MikuPan_EffectDebugEnumName(id));
            }
            else
            {
                igTextDisabled("%02d  %s", id,
                               MikuPan_EffectDebugEnumName(id));
            }

            igTableNextColumn();
            if (can_toggle)
            {
                igText("%s", MikuPan_EffectDebugFunctionName(id));
            }
            else
            {
                igTextDisabled("%s", MikuPan_EffectDebugFunctionName(id));
            }

            igTableNextColumn();
            const char* note = MikuPan_EffectDebugNote(id);
            if (note[0] != '\0')
            {
                igTextDisabled("%s", note);
            }
            else
            {
                igTextDisabled("Default test values");
            }

            igPopID();
        }

        igEndTable();
    }

    igEnd();
}

void MikuPan_UiDebugMenuRender(void)
{
    if (igBeginMenu("Debug", 1))
    {
        igCheckbox("Zero's Menu", (bool*) &dbg_wrk.mode_on);

        igSeparator();

        if (igBeginMenu("Rendering", 1))
        {
            igCheckbox("Draw Call Inspector", (bool*) &show_draw_inspector);
            igCheckbox("Shader Reload", (bool*) &show_shader_reload);
            igCheckbox("Effect Debug Window",
                       (bool*) &show_effect_debug_window);

            if (igBeginMenu("Visualization", 1))
            {
                igCheckbox("Wireframe", (bool*) &render_wireframe);
                igCheckbox("Bounding Boxes", (bool*) &show_bounding_boxes);
                igCheckbox("Normals", (bool*) &render_normals);

                if (render_normals)
                {
                    igSliderFloat("Normal Length", &normal_length, 0.1f, 100.0f,
                                  "%.1f", 0);
                }

                igCheckbox("Photo", (bool*) &show_photo_debug_window);

                igEndMenu();
            }

            if (igBeginMenu("Lighting", 1))
            {
                igCheckbox("Disable Lighting", (bool*) &disable_lighting);
                igCheckbox("Static Lighting", (bool*) &show_static_lighting);
                const char* lighting_modes[] = {"Per-Fragment", "Per-Vertex"};
                igCombo_Str_arr("Lighting Mode", &mikupan_configuration.renderer.lighting_mode,
                                lighting_modes, 2, -1);
                igEndMenu();
            }

            if (igBeginMenu("Shadows", 1))
            {
                int shadows_on = MikuPan_IsShadowEnabled();
                if (igCheckbox("Enable Shadows", (bool*) &shadows_on))
                {
                    MikuPan_SetShadowEnabled(shadows_on);
                }

                MikuPan_UiShadowResolutionCombo("Resolution");

                igCheckbox("Shadow Debug Window",
                           (bool*) &show_shadow_debug_window);
                igEndMenu();
            }

            if (igBeginMenu("Meshes", 1))
            {
                igCheckbox("Mesh 0x82", (bool*) &show_mesh_0x82);
                igCheckbox("Mesh 0x32", (bool*) &show_mesh_0x32);
                igCheckbox("Mesh 0x12", (bool*) &show_mesh_0x12);
                igCheckbox("Mesh 0x10", (bool*) &show_mesh_0x10);
                igCheckbox("Mesh 0x2", (bool*) &show_mesh_0x2);

                igSeparator();
                int mesh_cache_on = MikuPan_MeshCache_IsEnabled();

                if (igCheckbox("Mesh Cache", (bool*) &mesh_cache_on))
                {
                    MikuPan_MeshCache_SetEnabled(mesh_cache_on);
                }

                if (igButton("Clear Mesh Cache", (ImVec2) {0.0f, 0.0f}))
                {
                    MikuPan_MeshCache_Flush();
                }

                unsigned long long mesh_cache_bytes = 0;
                unsigned int mesh_cache_buffers = 0;
                unsigned int mesh_cache_entries = 0;
                MikuPan_MeshCache_GetStats(&mesh_cache_bytes,
                                            &mesh_cache_buffers,
                                            &mesh_cache_entries);
                igText("Cache: %u entries, %u buffers, %.1f MiB",
                       mesh_cache_entries,
                       mesh_cache_buffers,
                       (double)mesh_cache_bytes / (1024.0 * 1024.0));
                igText("GPU buffers: %u live / %u max",
                       MikuPan_GPUGetLiveBufferCount(),
                       MikuPan_GPUGetMaxBufferCount());

                extern int MikuPan_GpuSkinningEnabled(void);
                extern void MikuPan_SetGpuSkinningEnabled(int);
                int gpu_skin_on = MikuPan_GpuSkinningEnabled();
                if (igCheckbox("GPU Skinning (0x2/0xA)", (bool*) &gpu_skin_on))
                {
                    MikuPan_SetGpuSkinningEnabled(gpu_skin_on);
                }
                igEndMenu();
            }

            if (igBeginMenu("Textures", 1))
            {
                igCheckbox("Disable GS Uploads", (bool*) &disable_gs_uploads);

                if (igIsItemHovered(0))
                {
                    igBeginTooltip();
                    igText("THIS *WILL* BREAK TEXTURES, ONLY USE FOR TESTING PERFORMANCE IMPACT OF GS UPLOADS");
                    igEndTooltip();
                }

                igCheckbox("Texture List", (bool*) &show_texture_list);
                if (igButton("Clear Texture Cache", (ImVec2) {0.0f, 0.0f}))
                {
                    MikuPan_RequestFlushTextureCache();
                }

                igEndMenu();
            }

            igEndMenu();
        }

        if (igBeginMenu("Camera", 1))
        {
            bool first_person_enabled = MikuPan_FirstPersonEnabled() != 0;
            bool first_person_r3_toggle =
                mikupan_first_person_r3_toggle_enabled != 0;
            bool third_person_enabled = camera_third_person_enabled != 0;

            igSeparatorText("First Person");

            if (igCheckbox("First Person Mode", &first_person_enabled))
            {
                MikuPan_FirstPersonSetEnabled(first_person_enabled ? 1 : 0);
            }

            if (igCheckbox("R3 Toggles First Person", &first_person_r3_toggle))
            {
                mikupan_first_person_r3_toggle_enabled =
                    first_person_r3_toggle ? 1 : 0;
            }

            bool first_person_auto_run =
                mikupan_first_person_auto_run_enabled != 0;
            if (igCheckbox("Auto Run in First Person", &first_person_auto_run))
            {
                mikupan_first_person_auto_run_enabled =
                    first_person_auto_run ? 1 : 0;
            }

            igSliderFloat("FP Eye Height", &mikupan_first_person_eye_height,
                          600.0f, 800.0f, "%.0f", 0);
            igSliderFloat("FP Eye Forward", &mikupan_first_person_eye_forward,
                          -100.0f, 100.0f, "%.0f", 0);
            igSliderFloat("FP Look Distance",
                          &mikupan_first_person_look_distance, 250.0f, 2500.0f,
                          "%.0f", 0);
            igSliderFloat("FP FOV", &mikupan_first_person_fov_deg, 20.0f, 90.0f,
                          "%.1f deg", 0);
            igSliderFloat("FP Stick Yaw Speed",
                          &mikupan_first_person_stick_yaw_speed_deg, 0.1f,
                          8.0f, "%.2f deg/frame", 0);
            igSliderFloat("FP Stick Pitch Speed",
                          &mikupan_first_person_stick_pitch_speed_deg, 0.1f,
                          8.0f, "%.2f deg/frame", 0);

            igTextDisabled("Left stick strafes. Right stick/mouse aims.");

            igSeparatorText("Third Person");

            if (igCheckbox("Third Person Camera", &third_person_enabled))
            {
                camera_third_person_enabled = third_person_enabled ? 1 : 0;
            }

            igSliderFloat("TP Distance", &camera_third_person_distance, 100.0f,
                          2500.0f, "%.0f", 0);
            igSliderFloat("TP Height", &camera_third_person_height, 0.0f,
                          1400.0f, "%.0f", 0);
            igSliderFloat("TP Side", &camera_third_person_side, -600.0f, 600.0f,
                          "%.0f", 0);
            igSliderFloat("TP Look Ahead", &camera_third_person_look_ahead,
                          100.0f, 2500.0f, "%.0f", 0);
            igSliderFloat("TP Interest Height",
                          &camera_third_person_interest_height, -400.0f,
                          1200.0f, "%.0f", 0);
            igSliderFloat("TP FOV", &camera_third_person_fov_deg, 20.0f, 90.0f,
                          "%.1f deg", 0);

            igEndMenu();
        }

        if (igBeginMenu("Profiler", 1))
        {
            igCheckbox("FPS Counter", (bool*) &mikupan_configuration.show_fps);

            igCheckbox("Frame Time Graph", (bool*) &show_frame_time_graph);
            igEndMenu();
        }

        igEndMenu();
    }
}

void MikuPan_UiDebugWindowsRender(void)
{
    ImGuiIO* io = igGetIO_Nil();
    FrameTimeGraph_Update(&g_frame_graph, 1000.0f / io->Framerate,
                          MikuPan_GetLastFrameCpuMs(),
                          MikuPan_GetLastFrameGpuMs());

    if (dbg_wrk.mode_on == 1)
    {
        gra2dDrawDbgMenu();
    }

    if (show_frame_time_graph)
    {
        FrameTimeGraph_Draw(&g_frame_graph);
    }

    if (show_texture_list)
    {
        MikuPan_ShowTextureList();
    }

    MikuPan_UiShaderReloadWindow();
    MikuPan_UiEffectDebugWindow();
    MikuPan_UiDrawCallInspector();
    MikuPan_UiCameraDebugWindow();
    MikuPan_UiPhotoDebugWindow();
    MikuPan_UiShadowDebugWindow();

    if (mikupan_configuration.show_fps)
    {
        igBegin("fps", (bool*) &mikupan_configuration.show_fps, mikupan_no_navigation_window);
        //igPushFont(igGetFont(), 18.0f * ui_display_scale);
        igText("FPS %.2f", MikuPan_GetFrameRate());
        //igPopFont();
        igEnd();
    }
}

int MikuPan_IsBoundingBoxRendering(void)
{
    return show_bounding_boxes;
}

int MikuPan_ShowCameraDebug(void)
{
    return show_camera_debug;
}

int MikuPan_IsMesh0x82Rendering(void)
{
    return show_mesh_0x82;
}

int MikuPan_IsMesh0x32Rendering(void)
{
    return show_mesh_0x32;
}

int MikuPan_IsMesh0x12Rendering(void)
{
    return show_mesh_0x12;
}

int MikuPan_IsMesh0x10Rendering(void)
{
    return show_mesh_0x10;
}

int MikuPan_IsMesh0x2Rendering(void)
{
    return show_mesh_0x2;
}

int MikuPan_IsLightingDisabled(void)
{
    return disable_lighting;
}

int MikuPan_IsGsUploadsDisabled(void)
{
    return disable_gs_uploads;
}

int MikuPan_ShowStaticLighting(void)
{
    return show_static_lighting;
}

int MikuPan_GetMeshLightingMode(void)
{
    return mikupan_configuration.renderer.lighting_mode;
}

float MikuPan_GetNormalLength(void)
{
    return normal_length;
}
