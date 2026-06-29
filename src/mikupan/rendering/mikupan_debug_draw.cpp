#include "ingame/camera/camera.h"
#include "main/glob.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_ui_debug.h"
#include "mikupan_gpu.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include "mikupan_renderer_internal.h"
#include "mikupan_shader.h"

#include <math.h>

#define CAMERA_DEBUG_MAX_VERTS 24
#define CAMERA_DEBUG_RING_SEGS 12

static float debug_line_vertices[CAMERA_DEBUG_MAX_VERTS][4];
static int debug_line_vertex_count = 0;

static const float camera_marker_color[4]  = {1.0f, 0.78f, 0.10f, 0.95f};
static const float interest_marker_color[4] = {0.15f, 1.0f, 0.35f, 0.95f};
static const float view_line_color[4]       = {1.0f, 1.0f, 1.0f, 0.70f};
static const float camera_path_color[4]     = {1.0f, 0.42f, 0.08f, 0.90f};
static const float interest_path_color[4]   = {0.05f, 0.80f, 1.0f, 0.90f};
static const float target_color[4]          = {1.0f, 0.10f, 0.30f, 0.90f};

static void CameraDebugFlushLines(const float color[4])
{
    if (debug_line_vertex_count == 0)
    {
        return;
    }

    MikuPan_SetUniform4fvToCurrentShader((float *)color, "uColor");

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)(debug_line_vertex_count * sizeof(debug_line_vertices[0])),
        debug_line_vertices);
    MikuPan_TimedDrawArrays(GL_LINES, 0, debug_line_vertex_count);

    debug_line_vertex_count = 0;
}

static void CameraDebugAppendLine(const float a[4], const float b[4],
                                  const float color[4])
{
    if (debug_line_vertex_count + 2 > CAMERA_DEBUG_MAX_VERTS)
    {
        CameraDebugFlushLines(color);
    }

    debug_line_vertices[debug_line_vertex_count][0] = a[0];
    debug_line_vertices[debug_line_vertex_count][1] = a[1];
    debug_line_vertices[debug_line_vertex_count][2] = a[2];
    debug_line_vertices[debug_line_vertex_count][3] = 1.0f;
    debug_line_vertex_count++;

    debug_line_vertices[debug_line_vertex_count][0] = b[0];
    debug_line_vertices[debug_line_vertex_count][1] = b[1];
    debug_line_vertices[debug_line_vertex_count][2] = b[2];
    debug_line_vertices[debug_line_vertex_count][3] = 1.0f;
    debug_line_vertex_count++;
}

static void CameraDebugDrawLine(const float a[4], const float b[4],
                                const float color[4])
{
    CameraDebugAppendLine(a, b, color);
    CameraDebugFlushLines(color);
}

static void CameraDebugDrawCross(const float center[4], float radius,
                                 const float color[4])
{
    float a[4] = {center[0] - radius, center[1], center[2], 1.0f};
    float b[4] = {center[0] + radius, center[1], center[2], 1.0f};
    CameraDebugAppendLine(a, b, color);

    a[0] = center[0];
    a[1] = center[1] - radius;
    b[0] = center[0];
    b[1] = center[1] + radius;
    CameraDebugAppendLine(a, b, color);

    a[1] = center[1];
    a[2] = center[2] - radius;
    b[1] = center[1];
    b[2] = center[2] + radius;
    CameraDebugAppendLine(a, b, color);

    CameraDebugFlushLines(color);
}

static void CameraDebugRingPoint(float out[4], const float center[4],
                                 float radius, int plane, float angle)
{
    float c = cosf(angle) * radius;
    float s = sinf(angle) * radius;

    out[0] = center[0];
    out[1] = center[1];
    out[2] = center[2];
    out[3] = 1.0f;

    switch (plane)
    {
        case 0:
            out[1] += c;
            out[2] += s;
            break;
        case 1:
            out[0] += c;
            out[2] += s;
            break;
        default:
            out[0] += c;
            out[1] += s;
            break;
    }
}

static void CameraDebugDrawWireSphere(const float center[4], float radius,
                                      const float color[4])
{
    const float tau = 6.28318530718f;

    for (int plane = 0; plane < 3; plane++)
    {
        debug_line_vertex_count = 0;

        for (int i = 0; i < CAMERA_DEBUG_RING_SEGS; i++)
        {
            float p0[4];
            float p1[4];
            float a0 = (tau * (float)i) / (float)CAMERA_DEBUG_RING_SEGS;
            float a1 = (tau * (float)(i + 1)) / (float)CAMERA_DEBUG_RING_SEGS;

            CameraDebugRingPoint(p0, center, radius, plane, a0);
            CameraDebugRingPoint(p1, center, radius, plane, a1);
            CameraDebugAppendLine(p0, p1, color);
        }

        CameraDebugFlushLines(color);
    }
}

static void CameraDebugDrawPolyline(const sceVu0FVECTOR *points, int count,
                                    const float color[4])
{
    if (count < 2)
    {
        return;
    }

    for (int i = 1; i < count; i++)
    {
        CameraDebugAppendLine(points[i - 1], points[i], color);
    }

    CameraDebugFlushLines(color);
}

static int CameraDebugDifferent(const float a[4], const float b[4])
{
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return (dx * dx + dy * dy + dz * dz) > 4.0f;
}

void MikuPan_RenderLineLoop3D(const sceVu0FVECTOR *vertices, int count, u_char r,
                              u_char g, u_char b, u_char a)
{
    if (count < 2)
    {
        return;
    }

    float color[4] = {
        (float)r / 255.0f,
        (float)g / 255.0f,
        (float)b / 255.0f,
        (float)a / 255.0f,
    };

    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(BOUNDING_BOX_SHADER);
    MikuPan_SetUniform4fvToCurrentShader(color, "uColor");
    MikuPan_SetWorldSpaceModelTransform();
    MikuPan_SetRenderState3D();
    MikuPan_GPUSetCullNone();
    MikuPan_GPUSetDepthFunc(GL_ALWAYS);
    MikuPan_GPUSetDepthWrite(0);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)(count * sizeof(vertices[0])),
        vertices);
    MikuPan_TimedDrawArrays(GL_LINE_LOOP, 0, count);

    MikuPan_GPUSetDepthWrite(1);
    MikuPan_ResetRenderStateCache();
}

void MikuPan_RenderCameraDebug(void)
{
    if (!MikuPan_ShowCameraDebug())
    {
        return;
    }

    const CAMERA_DEBUG_PATH *path = CameraGetDebugPath();
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(CAMERA_DEBUG_SHADER);
    MikuPan_SetUniformMatrix4fvToCurrentShader((float *)camera.wcv, "uWorldClipView");
    MikuPan_SetRenderState3D();
    MikuPan_GPUSetCullNone();
    MikuPan_GPUSetDepthFunc(GL_ALWAYS);
    MikuPan_GPUSetDepthWrite(0);

    CameraDebugDrawLine(camera.p, camera.i, view_line_color);
    CameraDebugDrawCross(camera.p, 42.0f, camera_marker_color);
    CameraDebugDrawWireSphere(camera.p, 28.0f, camera_marker_color);
    CameraDebugDrawCross(camera.i, 46.0f, interest_marker_color);
    CameraDebugDrawWireSphere(camera.i, 36.0f, interest_marker_color);

    if (path->active)
    {
        CameraDebugDrawPolyline(path->camera_path,
                                path->camera_path_points,
                                camera_path_color);
        CameraDebugDrawPolyline(path->interest_path,
                                path->interest_path_points,
                                interest_path_color);

        for (int i = 0; i < path->camera_path_points; i++)
        {
            CameraDebugDrawCross(path->camera_path[i], 28.0f, camera_path_color);
        }

        for (int i = 0; i < path->interest_path_points; i++)
        {
            CameraDebugDrawCross(path->interest_path[i], 28.0f, interest_path_color);
        }

        if (CameraDebugDifferent(camera.p, path->target_p))
        {
            CameraDebugDrawLine(camera.p, path->target_p, target_color);
            CameraDebugDrawWireSphere(path->target_p, 22.0f, target_color);
        }

        if (CameraDebugDifferent(camera.i, path->target_i))
        {
            CameraDebugDrawLine(camera.i, path->target_i, interest_path_color);
            CameraDebugDrawWireSphere(path->target_i, 22.0f, interest_path_color);
        }
    }

    MikuPan_GPUSetDepthWrite(1);
    MikuPan_ResetRenderStateCache();
}
