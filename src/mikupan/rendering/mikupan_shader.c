#include "mikupan_shader.h"

#include "glad/gl.h"
#include "mikupan/mikupan_file_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GLuint current_program = 0;
GLuint backup_current_program = 0;
u_int shader_list[MAX_SHADER_PROGRAMS] = {0};
static unsigned int g_shader_generation = 0;

/// Per-pass shader override. When >= 0, MikuPan_SetCurrentShaderProgram
/// substitutes this index for whatever the caller asked for. The shadow
/// pass uses it to redirect every mesh-type renderer onto the silhouette
/// shader without modifying their internals.
static int g_shader_override = -1;

/// Cached uniform locations per shader. glGetUniformLocation is a string lookup
/// in the driver and is slow; we resolve the hot uniforms once at link time.
typedef struct
{
    GLint model;
    GLint view;
    GLint projection;
    /// Precomputed derived matrices — pushed once per object instead of being
    /// recomputed inside the vertex shader for every vertex.
    GLint mvp;             ///< projection * view * model
    GLint modelView;       ///< view * model
    GLint viewProj;        ///< projection * view (no model factor; used by 0xA)
    GLint normalMatrix;    ///< transpose(inverse(view * model)) — mat3
    GLint viewNormalMatrix;///< transpose(inverse(view))         — mat3 (0xA)

    /* Non-matrix uniforms */
    GLint uColor;         ///< bounding_box.vert
    GLint renderNormals;  ///< textured_mesh_lighted.frag
    GLint uNormalLength;  ///< normals_debug.geom
    GLint uFog;           ///< textured_mesh_lighted.frag
    GLint uFogColor;      ///< textured_mesh_lighted.frag
    GLint uMeshLightingMode;///< textured_mesh_lighted.* per-fragment/per-vertex toggle
    GLint disableLighting;///< textured_mesh_lighted.frag — UI debug toggle
} CachedUniformLocations;

static CachedUniformLocations uniform_loc[MAX_SHADER_PROGRAMS] = {0};

// [vert, geom (NULL = none), frag]
const char *shader_file_name[MAX_SHADER_PROGRAMS][3] = {
    {                  "./resources/shaders/mesh_0x2.vert",NULL,
     "./resources/shaders/textured_mesh_lighted.frag"                                             },
    {                  "./resources/shaders/mesh_0xA.vert", NULL,
     "./resources/shaders/textured_mesh_lighted.frag"                                             },
    {                 "./resources/shaders/mesh_0x12.vert", NULL,
     "./resources/shaders/textured_mesh_lighted.frag"                                             },
    {"./resources/shaders/untextured_coloured_sprite.vert", NULL,
     "./resources/shaders/untextured_coloured_sprite.frag"                                        },
    {              "./resources/shaders/bounding_box.vert", NULL,
     "./resources/shaders/untextured_coloured_sprite.frag"                                        },
    {                    "./resources/shaders/sprite.vert", NULL,
     "./resources/shaders/sprite.frag"                                                            },
    {                 "./resources/shaders/mesh_0x12.vert",
     "./resources/shaders/normals_debug.geom",            "./resources/shaders/normals_debug.frag"},
    {                  "./resources/shaders/mesh_0x2.vert",
     "./resources/shaders/normals_debug.geom",            "./resources/shaders/normals_debug.frag"},
    // Final scene-to-window blit. Vertex layout matches sprite.vert
    // (UV4_COLOUR4_POSITION4) so the same fullscreen-quad VBO drives it.
    {                    "./resources/shaders/sprite.vert", NULL,
     "./resources/shaders/postprocess.frag"                                                       },
    // Shadow blob (debug fallback) — fullscreen-quad-style ellipse fill into
    // the shadow FBO. Matches sprite.vert layout so the same fullscreen quad
    // VBO drives it. Useful when no caster mesh is wired up yet.
    {                    "./resources/shaders/sprite.vert", NULL,
     "./resources/shaders/shadow_caster.frag"                                                     },
    // Shadow silhouette — position-only vert + constant alpha frag. Bound by
    // the shadow pass's shader-override mechanism, so the caster meshes drive
    // it through the regular MikuPan_RenderMeshType* paths with whatever
    // vertex layout they normally use (location 0 = position).
    {         "./resources/shaders/shadow_silhouette.vert", NULL,
     "./resources/shaders/shadow_silhouette.frag"                                                 },
    {             "./resources/shaders/camera_debug.vert", NULL,
     "./resources/shaders/untextured_coloured_sprite.frag"                                        },
};

static const char *kShaderNames[MAX_SHADER_PROGRAMS] = {
    "MESH_0x2",          "MESH_0xA",
    "MESH_0x12",         "UNTEXTURED_COLOURED_SPRITE",
    "BOUNDING_BOX",      "SPRITE",
    "NORMALS_0x12",      "NORMALS_0x2",
    "POSTPROCESS",       "SHADOW_BLOB",
    "SHADOW_SILHOUETTE", "CAMERA_DEBUG",
};

/// Linear scan — MAX_SHADER_PROGRAMS is 8, so this is faster than maintaining
/// a parallel "current index" tracker that has to stay in sync with external
/// glUseProgram calls (e.g. via MikuPan_ResetShaderCache).
static int FindShaderIndex(GLuint program)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        if (shader_list[i] == program)
            return i;
    }
    return -1;
}

static GLint GetCachedLocation(int idx, const char *name)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return -1;
    }
    const CachedUniformLocations *u = &uniform_loc[idx];

    if (strcmp(name, "model") == 0)
        return u->model;
    if (strcmp(name, "view") == 0)
        return u->view;
    if (strcmp(name, "projection") == 0)
        return u->projection;
    if (strcmp(name, "mvp") == 0)
        return u->mvp;
    if (strcmp(name, "modelView") == 0)
        return u->modelView;
    if (strcmp(name, "viewProj") == 0)
        return u->viewProj;
    if (strcmp(name, "normalMatrix") == 0)
        return u->normalMatrix;
    if (strcmp(name, "viewNormalMatrix") == 0)
        return u->viewNormalMatrix;
    if (strcmp(name, "uColor") == 0)
        return u->uColor;
    if (strcmp(name, "renderNormals") == 0)
        return u->renderNormals;
    if (strcmp(name, "uNormalLength") == 0)
        return u->uNormalLength;
    if (strcmp(name, "uFog") == 0)
        return u->uFog;
    if (strcmp(name, "uFogColor") == 0)
        return u->uFogColor;
    if (strcmp(name, "disableLighting") == 0)
        return u->disableLighting;
    if (strcmp(name, "uMeshLightingMode") == 0)
        return u->uMeshLightingMode;

    return glad_glGetUniformLocation(shader_list[idx], name);
}

const char *MikuPan_GetShaderName(int idx)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return "<unknown>";
    }

    return kShaderNames[idx];
}

const char *MikuPan_GetShaderVertSource(int idx)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return NULL;
    }

    return shader_file_name[idx][0];
}

const char *MikuPan_GetShaderGeomSource(int idx)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return NULL;
    }

    return shader_file_name[idx][1];
}

const char *MikuPan_GetShaderFragSource(int idx)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return NULL;
    }

    return shader_file_name[idx][2];
}

static void CacheUniformLocations(int idx, GLuint program)
{
    uniform_loc[idx].model =                glad_glGetUniformLocation(program, "model");
    uniform_loc[idx].view =                 glad_glGetUniformLocation(program, "view");
    uniform_loc[idx].projection =           glad_glGetUniformLocation(program, "projection");
    uniform_loc[idx].mvp =                  glad_glGetUniformLocation(program, "mvp");
    uniform_loc[idx].modelView =            glad_glGetUniformLocation(program, "modelView");
    uniform_loc[idx].viewProj =             glad_glGetUniformLocation(program, "viewProj");
    uniform_loc[idx].normalMatrix =         glad_glGetUniformLocation(program, "normalMatrix");
    uniform_loc[idx].viewNormalMatrix =     glad_glGetUniformLocation(program, "viewNormalMatrix");
    uniform_loc[idx].uColor =               glad_glGetUniformLocation(program, "uColor");
    uniform_loc[idx].renderNormals =        glad_glGetUniformLocation(program, "renderNormals");
    uniform_loc[idx].uNormalLength =        glad_glGetUniformLocation(program, "uNormalLength");
    uniform_loc[idx].uFog =                 glad_glGetUniformLocation(program, "uFog");
    uniform_loc[idx].uFogColor =            glad_glGetUniformLocation(program, "uFogColor");
    uniform_loc[idx].disableLighting =      glad_glGetUniformLocation(program, "disableLighting");
    uniform_loc[idx].uMeshLightingMode =    glad_glGetUniformLocation(program, "uMeshLightingMode");
}

static GLuint CompileStageFromFile(GLenum stage, const char *path,
                                   char *err_buf, int err_buf_size)
{
    u_int file_size = MikuPan_GetFileSize(path) + 1;
    char *src = (char *) malloc(file_size);

    if (src == NULL)
    {
        if (err_buf && err_buf_size > 0)
        {
            snprintf(err_buf, err_buf_size, "OOM reading %s", path);
        }

        return 0;
    }

    src[file_size - 1] = 0;
    MikuPan_ReadFullFile(path, src);

    GLuint sh = glad_glCreateShader(stage);
    glad_glShaderSource(sh, 1, (const GLchar *const *) &src, NULL);
    glad_glCompileShader(sh);
    free(src);

    GLint ok = GL_FALSE;
    glad_glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        if (err_buf && err_buf_size > 0)
        {
            int written = snprintf(err_buf, err_buf_size, "%s: ", path);

            if (written < err_buf_size - 1)
            {
                glad_glGetShaderInfoLog(sh, err_buf_size - written, NULL,
                                        err_buf + written);
            }
        }

        glad_glDeleteShader(sh);
        return 0;
    }

    return sh;
}

/// Build a fresh GL program for shader index `idx` from its on-disk sources.
/// Returns 0 on failure (with an error message in err_buf when provided).
/// Does NOT touch shader_list[idx] / uniform_loc[idx] — caller decides when
/// to swap so a failed reload leaves the live program untouched.
static GLuint BuildShaderProgram(int idx, char *err_buf, int err_buf_size)
{
    const char *vert_path = shader_file_name[idx][0];
    const char *geom_path = shader_file_name[idx][1];
    const char *frag_path = shader_file_name[idx][2];

    GLuint vs = CompileStageFromFile(GL_VERTEX_SHADER, vert_path, err_buf,
                                     err_buf_size);
    if (!vs)
    {
        return 0;
    }

    GLuint fs = CompileStageFromFile(GL_FRAGMENT_SHADER, frag_path, err_buf,
                                     err_buf_size);
    if (!fs)
    {
        glad_glDeleteShader(vs);
        return 0;
    }

    GLuint gs = 0;
    if (geom_path != NULL)
    {
        gs = CompileStageFromFile(GL_GEOMETRY_SHADER, geom_path, err_buf,
                                  err_buf_size);
        if (!gs)
        {
            glad_glDeleteShader(vs);
            glad_glDeleteShader(fs);
            return 0;
        }
    }

    GLuint prog = glad_glCreateProgram();
    glad_glAttachShader(prog, vs);
    glad_glAttachShader(prog, fs);

    if (gs)
    {
        glad_glAttachShader(prog, gs);
    }

    glad_glLinkProgram(prog);

    glad_glDeleteShader(vs);
    glad_glDeleteShader(fs);

    if (gs)
    {
        glad_glDeleteShader(gs);
    }

    GLint ok = GL_FALSE;
    glad_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        if (err_buf && err_buf_size > 0)
        {
            int written = snprintf(err_buf, err_buf_size, "%s link: ", kShaderNames[idx]);

            if (written < err_buf_size - 1)
            {
                glad_glGetProgramInfoLog(prog, err_buf_size - written, NULL,
                                         err_buf + written);
            }
        }

        glad_glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

int MikuPan_InitShaders()
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        GLuint prog = BuildShaderProgram(i, NULL, 0);
        if (prog == 0)
        {
            continue;
        }

        shader_list[i] = prog;
        current_program = prog;
        CacheUniformLocations(i, prog);
    }

    glad_glUseProgram(current_program);
    g_shader_generation++;

    return 0;
}

int MikuPan_ReloadShader(int idx, char *err_buf, int err_buf_size)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        if (err_buf && err_buf_size > 0)
        {
            snprintf(err_buf, err_buf_size, "bad shader index %d", idx);
        }

        return -1;
    }

    GLuint prog = BuildShaderProgram(idx, err_buf, err_buf_size);
    if (prog == 0)
    {
        return -1;
    }

    GLuint old = shader_list[idx];
    shader_list[idx] = prog;
    CacheUniformLocations(idx, prog);

    // If the live program is the one we replaced, swap it on the GL side too.
    if (current_program == old)
    {
        current_program = prog;
        glad_glUseProgram(current_program);
    }

    if (old != 0)
    {
        glad_glDeleteProgram(old);
    }

    if (err_buf && err_buf_size > 0)
    {
        err_buf[0] = '\0';
    }

    g_shader_generation++;

    return 0;
}

int MikuPan_ReloadAllShaders(char *err_buf, int err_buf_size)
{
    int any_failed = 0;
    char local_err[1024];

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        local_err[0] = '\0';
        if (MikuPan_ReloadShader(i, local_err, (int) sizeof(local_err)) != 0)
        {
            // Keep the first error; later failures stay in the log only.
            if (!any_failed && err_buf && err_buf_size > 0)
            {
                snprintf(err_buf, err_buf_size, "%s", local_err);
            }

            any_failed = 1;
        }
    }

    return any_failed ? -1 : 0;
}

u_int MikuPan_SetCurrentShaderProgram(int shader_program)
{
    // Per-pass override — used by the shadow pass to redirect every mesh
    // renderer onto SHADOW_SILHOUETTE_SHADER without touching their bodies.
    if (g_shader_override >= 0 && g_shader_override < MAX_SHADER_PROGRAMS)
    {
        shader_program = g_shader_override;
    }

    if (shader_program >= MAX_SHADER_PROGRAMS)
    {
        return -1;
    }

    GLuint new_program = shader_list[shader_program];
    if (new_program != current_program)
    {
        current_program = new_program;
        glad_glUseProgram(current_program);
    }
    return current_program;
}

void MikuPan_SetShaderOverride(int shader_index)
{
    g_shader_override = shader_index;
}

int MikuPan_GetShaderOverride(void)
{
    return g_shader_override;
}

u_int MikuPan_GetCurrentShaderProgram()
{
    return current_program;
}

void MikuPan_SetUniformMatrix4fvToAllShaders(float *mat, char *name)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        GLint loc = GetCachedLocation(i, name);
        if (loc < 0)
        {
            continue;
        }
        MikuPan_SetCurrentShaderProgram(i);
        glad_glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
    }
}

void MikuPan_SetUniformMatrix3fvToAllShaders(float *mat, char *name)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        GLint loc = GetCachedLocation(i, name);
        if (loc < 0)
        {
            continue;
        }

        MikuPan_SetCurrentShaderProgram(i);
        glad_glUniformMatrix3fv(loc, 1, GL_FALSE, mat);
    }
}

void MikuPan_SetUniformMatrix4fvToCurrentShader(float *mat, char *name)
{
    GLint loc = GetCachedLocation(FindShaderIndex(current_program), name);
    if (loc < 0)
    {
        return;
    }

    glad_glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
}

void MikuPan_SetUniformMatrix3fvToCurrentShader(float *mat, char *name)
{
    GLint loc = GetCachedLocation(FindShaderIndex(current_program), name);
    if (loc < 0)
    {
        return;
    }

    glad_glUniformMatrix3fv(loc, 1, GL_FALSE, mat);
}

void MikuPan_SetUniform4fvToAllShaders(float *vector, char *name)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        GLint loc = GetCachedLocation(i, name);
        if (loc < 0)
        {
            continue;
        }

        MikuPan_SetCurrentShaderProgram(i);
        glad_glUniform4fv(loc, 1, vector);
    }
}

void MikuPan_SetUniform4fvToCurrentShader(float *vector, char *name)
{
    GLint loc = GetCachedLocation(FindShaderIndex(current_program), name);

    if (loc < 0)
    {
        return;
    }

    glad_glUniform4fv(loc, 1, vector);
}

void MikuPan_SetUniform1iToAllShaders(int value, char *name)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        GLint loc = GetCachedLocation(i, name);

        if (loc < 0)
        {
            continue;
        }

        MikuPan_SetCurrentShaderProgram(i);
        glad_glUniform1i(loc, value);
    }
}

void MikuPan_SetUniform1iToCurrentShader(int value, char *name)
{
    GLint loc = GetCachedLocation(FindShaderIndex(current_program), name);
    if (loc < 0)
    {
        return;
    }

    glad_glUniform1i(loc, value);
}

void MikuPan_SetUniform1fToAllShaders(float value, char *name)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        GLint loc = GetCachedLocation(i, name);
        if (loc < 0)
        {
            continue;
        }

        MikuPan_SetCurrentShaderProgram(i);
        glad_glUniform1f(loc, value);
    }
}

void MikuPan_SetUniform1fToCurrentShader(float value, char *name)
{
    GLint loc = GetCachedLocation(FindShaderIndex(current_program), name);
    if (loc < 0)
    {
        return;
    }

    glad_glUniform1f(loc, value);
}

void MikuPan_SetUniform2fToCurrentShader(float x, float y, char *name)
{
    GLint loc = GetCachedLocation(FindShaderIndex(current_program), name);
    if (loc < 0)
    {
        return;
    }

    glad_glUniform2f(loc, x, y);
}

void MikuPan_ResetShaderCache(void)
{
    current_program = 0;
}

unsigned int MikuPan_GetShaderGeneration(void)
{
    return g_shader_generation;
}
