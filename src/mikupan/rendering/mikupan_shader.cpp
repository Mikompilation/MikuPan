#include "mikupan_shader.h"

#include "mikupan/io/mikupan_file_c.h"
#include "mikupan/debug/mikupan_logging.h"
#include "mikupan_gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u_int current_program = 0;
u_int backup_current_program = 0;
u_int shader_list[MAX_SHADER_PROGRAMS] = {0};
static unsigned int g_shader_generation = 0;
static int g_shader_override = -1;

static SDL_GPUShader *g_vertex_shaders[MAX_SHADER_PROGRAMS] = {0};
static SDL_GPUShader *g_fragment_shaders[MAX_SHADER_PROGRAMS] = {0};

// [vert, geom (NULL = none; SDL_GPU has no geometry stage), frag]
const char *shader_file_name[MAX_SHADER_PROGRAMS][3] = {
    {"resources/shaders/hlsl/mesh_0x2.vert.hlsl", NULL,
     "resources/shaders/hlsl/textured_mesh_lighted.frag.hlsl"},
    {"resources/shaders/hlsl/mesh_0xA.vert.hlsl", NULL,
     "resources/shaders/hlsl/textured_mesh_lighted.frag.hlsl"},
    {"resources/shaders/hlsl/mesh_0x12.vert.hlsl", NULL,
     "resources/shaders/hlsl/textured_mesh_lighted.frag.hlsl"},
    {"resources/shaders/hlsl/untextured_coloured_sprite.vert.hlsl", NULL,
     "resources/shaders/hlsl/untextured_coloured_sprite.frag.hlsl"},
    {"resources/shaders/hlsl/bounding_box.vert.hlsl", NULL,
     "resources/shaders/hlsl/untextured_coloured_sprite.frag.hlsl"},
    {"resources/shaders/hlsl/sprite.vert.hlsl", NULL,
     "resources/shaders/hlsl/sprite.frag.hlsl"},
    {"resources/shaders/hlsl/sprite.vert.hlsl", NULL,
     "resources/shaders/hlsl/sprite_alpha_as_rgb.frag.hlsl"},
    {"resources/shaders/hlsl/normals_0x12.vert.hlsl", NULL,
     "resources/shaders/hlsl/untextured_coloured_sprite.frag.hlsl"},
    {"resources/shaders/hlsl/normals_0x2.vert.hlsl", NULL,
     "resources/shaders/hlsl/untextured_coloured_sprite.frag.hlsl"},
    {"resources/shaders/hlsl/sprite.vert.hlsl", NULL,
     "resources/shaders/hlsl/postprocess.frag.hlsl"},
    {"resources/shaders/hlsl/sprite.vert.hlsl", NULL,
     "resources/shaders/hlsl/shadow_caster.frag.hlsl"},
    {"resources/shaders/hlsl/shadow_silhouette.vert.hlsl", NULL,
     "resources/shaders/hlsl/shadow_silhouette.frag.hlsl"},
    {"resources/shaders/hlsl/shadow_receiver.vert.hlsl", NULL,
     "resources/shaders/hlsl/shadow_receiver.frag.hlsl"},
    {"resources/shaders/hlsl/camera_debug.vert.hlsl", NULL,
     "resources/shaders/hlsl/untextured_coloured_sprite.frag.hlsl"},
    {"resources/shaders/hlsl/heat_haze.vert.hlsl", NULL,
     "resources/shaders/hlsl/heat_haze.frag.hlsl"},
    {"resources/shaders/hlsl/mesh_0x2_skinned.vert.hlsl", NULL,
     "resources/shaders/hlsl/textured_mesh_lighted.frag.hlsl"},
    {"resources/shaders/hlsl/mesh_0xA_skinned.vert.hlsl", NULL,
     "resources/shaders/hlsl/textured_mesh_lighted.frag.hlsl"},
};

static const char *kShaderNames[MAX_SHADER_PROGRAMS] = {
    "MESH_0x2", "MESH_0xA",
    "MESH_0x12", "UNTEXTURED_COLOURED_SPRITE",
    "BOUNDING_BOX", "SPRITE", "SPRITE_ALPHA_AS_RGB",
    "NORMALS_0x12", "NORMALS_0x2",
    "POSTPROCESS", "SHADOW_BLOB",
    "SHADOW_SILHOUETTE", "SHADOW_RECEIVER",
    "CAMERA_DEBUG", "HEAT_HAZE",
    "MESH_0x2_SKINNED", "MESH_0xA_SKINNED",
};

typedef struct
{
    SDL_GPUShaderFormat format;
    const char *dir;       ///< subdirectory under resources/shaders/
    const char *extension; ///< bytecode file extension, with dot
    const char *entrypoint;
} MikuPan_ShaderFormatInfo;

/* Every format the build produces (cmake/shadercross.cmake), ordered by
 * preference when a device supports several. The MSL entry point is "main0":
 * SPIRV-Cross renames main, which is a reserved word in MSL. */
static const MikuPan_ShaderFormatInfo kShaderFormats[] = {
    {SDL_GPU_SHADERFORMAT_SPIRV, "spirv", ".spv", "main"},
    {SDL_GPU_SHADERFORMAT_DXIL, "dxil", ".dxil", "main"},
    {SDL_GPU_SHADERFORMAT_MSL, "msl", ".msl", "main0"},
};

static const MikuPan_ShaderFormatInfo *PickShaderFormat(void)
{
    SDL_GPUShaderFormat supported =
        SDL_GetGPUShaderFormats(MikuPan_GPUGetDevice());

    for (size_t i = 0; i < SDL_arraysize(kShaderFormats); i++)
    {
        if (supported & kShaderFormats[i].format)
        {
            return &kShaderFormats[i];
        }
    }

    return NULL;
}

static int BuildShaderBytecodePath(const char *source_path,
                                   const MikuPan_ShaderFormatInfo *format,
                                   char *out_path,
                                   int out_path_size)
{
    char relative_path[512];
    const char *name = source_path;
    const char *slash = strrchr(source_path, '/');
    const char *backslash = strrchr(source_path, '\\');
    size_t len;

    if (backslash != NULL && (slash == NULL || backslash > slash))
    {
        slash = backslash;
    }
    if (slash != NULL)
    {
        name = slash + 1;
    }

    len = strlen(name);
    if (len > 5 && strcmp(name + len - 5, ".hlsl") == 0)
    {
        len -= 5;
    }

    int written = snprintf(relative_path, sizeof(relative_path),
                           "resources/shaders/%s/%.*s%s",
                           format->dir, (int)len, name,
                           format->extension);
    if (written < 0 || written >= (int)sizeof(relative_path))
    {
        return 0;
    }

    return MikuPan_ResolveBasePath(relative_path, out_path,
                                   (size_t)out_path_size);
}

static Uint8 *ReadShaderBytecode(const char *path,
                                 size_t *out_size,
                                 char *err_buf,
                                 int err_buf_size)
{
    u_int file_size = MikuPan_GetFileSize(path);
    Uint8 *data;

    if (file_size == 0)
    {
        if (err_buf && err_buf_size > 0)
        {
            snprintf(err_buf, err_buf_size, "Missing shader bytecode: %s",
                     path);
        }
        return NULL;
    }

    data = (Uint8 *)malloc(file_size);
    if (data == NULL)
    {
        if (err_buf && err_buf_size > 0)
        {
            snprintf(err_buf, err_buf_size, "OOM reading %s", path);
        }
        return NULL;
    }

    MikuPan_ReadFullFile(path, (char *)data);
    *out_size = file_size;
    return data;
}

static SDL_GPUShader *CompileStageFromFile(const char *path,
                                           SDL_GPUShaderStage stage,
                                           int num_storage_buffers,
                                           char *err_buf,
                                           int err_buf_size)
{
    char bytecode_path[512];
    size_t bytecode_size = 0;
    Uint8 *bytecode;

    const MikuPan_ShaderFormatInfo *format = PickShaderFormat();
    if (format == NULL)
    {
        if (err_buf && err_buf_size > 0)
        {
            snprintf(err_buf, err_buf_size,
                     "No shipped shader format matches the GPU device");
        }
        return NULL;
    }

    if (!BuildShaderBytecodePath(path, format, bytecode_path,
                                 (int)sizeof(bytecode_path)))
    {
        if (err_buf && err_buf_size > 0)
        {
            snprintf(err_buf, err_buf_size,
                     "Shader bytecode path too long for %s", path);
        }
        return NULL;
    }

    bytecode = ReadShaderBytecode(bytecode_path, &bytecode_size,
                                  err_buf, err_buf_size);
    if (bytecode == NULL)
    {
        return NULL;
    }

    SDL_GPUShaderCreateInfo create_info = {0};
    create_info.code_size = bytecode_size;
    create_info.code = bytecode;
    create_info.entrypoint = format->entrypoint;
    create_info.format = format->format;
    create_info.stage = stage;
    create_info.num_samplers =
        stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 2u : 0u;
    create_info.num_storage_textures = 0;
    create_info.num_storage_buffers = (Uint32)num_storage_buffers;
    create_info.num_uniform_buffers = 3;

    SDL_GPUShader *shader =
        SDL_CreateGPUShader(MikuPan_GPUGetDevice(), &create_info);

    free(bytecode);

    if (shader == NULL && err_buf && err_buf_size > 0)
    {
        snprintf(err_buf, err_buf_size, "%s: SDL_GPU shader compile failed: %s",
                 path, SDL_GetError());
    }

    return shader;
}

static int BuildShaderProgram(int idx,
                              SDL_GPUShader **out_vertex,
                              SDL_GPUShader **out_fragment,
                              char *err_buf,
                              int err_buf_size)
{
    const char *vert_path = shader_file_name[idx][0];
    const char *frag_path = shader_file_name[idx][2];

    /// The GPU-skinned mesh shaders read the bone-matrix palette from one
    /// vertex storage buffer; every other shader uses none.
    int vert_storage_buffers =
        (idx == MESH_0x2_SKINNED_SHADER || idx == MESH_0xA_SKINNED_SHADER) ? 1 : 0;

    SDL_GPUShader *vs = CompileStageFromFile(
        vert_path, SDL_GPU_SHADERSTAGE_VERTEX,
        vert_storage_buffers, err_buf, err_buf_size);
    if (vs == NULL)
    {
        return -1;
    }

    SDL_GPUShader *fs = CompileStageFromFile(
        frag_path, SDL_GPU_SHADERSTAGE_FRAGMENT,
        0, err_buf, err_buf_size);
    if (fs == NULL)
    {
        SDL_ReleaseGPUShader(MikuPan_GPUGetDevice(), vs);
        return -1;
    }

    *out_vertex = vs;
    *out_fragment = fs;
    return 0;
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

int MikuPan_InitShaders()
{
    int failed = 0;

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        SDL_GPUShader *vs = NULL;
        SDL_GPUShader *fs = NULL;
        char err_buf[512] = "";
        if (BuildShaderProgram(i, &vs, &fs, err_buf,
                               (int)sizeof(err_buf)) != 0)
        {
            info_log("Failed to load shader %s: %s",
                     MikuPan_GetShaderName(i),
                     err_buf[0] != '\0' ? err_buf : "unknown error");
            failed = 1;
            continue;
        }

        g_vertex_shaders[i] = vs;
        g_fragment_shaders[i] = fs;
        shader_list[i] = (u_int)(i + 1);
        current_program = shader_list[i];
    }

    if (failed)
    {
        return -1;
    }

    if (current_program != 0)
    {
        MikuPan_GPUSetCurrentShader((int)current_program - 1);
    }
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

    SDL_GPUShader *vs = NULL;
    SDL_GPUShader *fs = NULL;
    if (BuildShaderProgram(idx, &vs, &fs, err_buf, err_buf_size) != 0)
    {
        return -1;
    }

    SDL_GPUDevice *device = MikuPan_GPUGetDevice();
    SDL_GPUShader *old_vs = g_vertex_shaders[idx];
    SDL_GPUShader *old_fs = g_fragment_shaders[idx];

    g_vertex_shaders[idx] = vs;
    g_fragment_shaders[idx] = fs;
    shader_list[idx] = (u_int)(idx + 1);
    MikuPan_GPUInvalidatePipelines();

    if (current_program == (u_int)(idx + 1))
    {
        MikuPan_GPUSetCurrentShader(idx);
    }

    if (old_vs != NULL)
    {
        SDL_ReleaseGPUShader(device, old_vs);
    }
    if (old_fs != NULL)
    {
        SDL_ReleaseGPUShader(device, old_fs);
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
        if (MikuPan_ReloadShader(i, local_err, (int)sizeof(local_err)) != 0)
        {
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
    if (g_shader_override >= 0 && g_shader_override < MAX_SHADER_PROGRAMS)
    {
        if (g_shader_override == SHADOW_SILHOUETTE_SHADER &&
            (shader_program == MESH_0x2_SHADER ||
             shader_program == MESH_0xA_SHADER ||
             shader_program == MESH_0x12_SHADER ||
             shader_program == MESH_0x2_SKINNED_SHADER ||
             shader_program == MESH_0xA_SKINNED_SHADER))
        {
            // Keep requested textured mesh shader c:
        }
        else
        {
            shader_program = g_shader_override;
        }
    }

    if (shader_program < 0 || shader_program >= MAX_SHADER_PROGRAMS)
    {
        return (u_int)-1;
    }

    current_program = shader_list[shader_program];
    MikuPan_GPUSetCurrentShader(shader_program);
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
    MikuPan_GPUSetMatrix4(name, mat);
}

void MikuPan_SetUniformMatrix3fvToAllShaders(float *mat, char *name)
{
    MikuPan_GPUSetMatrix3(name, mat);
}

void MikuPan_SetUniformMatrix4fvToCurrentShader(float *mat, char *name)
{
    MikuPan_GPUSetMatrix4(name, mat);
}

void MikuPan_SetUniformMatrix3fvToCurrentShader(float *mat, char *name)
{
    MikuPan_GPUSetMatrix3(name, mat);
}

void MikuPan_SetUniform4fvToAllShaders(float *vector, char *name)
{
    MikuPan_GPUSetVec4(name, vector);
}

void MikuPan_SetUniform4fvToCurrentShader(float *vector, char *name)
{
    MikuPan_GPUSetVec4(name, vector);
}

void MikuPan_SetUniform1iToAllShaders(int value, char *name)
{
    MikuPan_GPUSetInt(name, value);
}

void MikuPan_SetUniform1iToCurrentShader(int value, char *name)
{
    MikuPan_GPUSetInt(name, value);
}

void MikuPan_SetUniform1fToAllShaders(float value, char *name)
{
    MikuPan_GPUSetFloat(name, value);
}

void MikuPan_SetUniform1fToCurrentShader(float value, char *name)
{
    MikuPan_GPUSetFloat(name, value);
}

void MikuPan_SetUniform2fToCurrentShader(float x, float y, char *name)
{
    MikuPan_GPUSetVec2(name, x, y);
}

void MikuPan_ResetShaderCache(void)
{
    current_program = 0;
}

unsigned int MikuPan_GetShaderGeneration(void)
{
    return g_shader_generation;
}

SDL_GPUShader *MikuPan_GetGPUVertexShader(int idx)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return NULL;
    }
    return g_vertex_shaders[idx];
}

SDL_GPUShader *MikuPan_GetGPUFragmentShader(int idx)
{
    if (idx < 0 || idx >= MAX_SHADER_PROGRAMS)
    {
        return NULL;
    }
    return g_fragment_shaders[idx];
}
