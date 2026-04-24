#include "mikupan_shader.h"

#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_logging_c.h"
#include <SDL3_shadercross/SDL_shadercross.h>

#include <stdlib.h>
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Global state definitions
// ─────────────────────────────────────────────────────────────────────────────

MikuPan_MeshVertUniforms  g_vert_uniforms     = {0};
MikuPan_BBVertUniforms    g_bb_uniforms       = {0};
MikuPan_FragMiscUniforms  g_frag_misc_uniforms = {
    .renderNormals = 0,
    .uColorScale   = 1.0f,
    ._pad          = {0.0f, 0.0f},
    .uFog          = {0.0f, 1.0f, 1.0f, 0.0f},
    .uFogColor     = {0.0f, 0.0f, 0.0f, 1.0f}
};

SDL_GPUShader *g_vertex_shaders[MAX_SHADER_PROGRAMS]   = {0};
SDL_GPUShader *g_fragment_shaders[MAX_SHADER_PROGRAMS] = {0};

int g_current_shader_type = 0;

// ─────────────────────────────────────────────────────────────────────────────
// SPIRV file paths:  [shader_index][0] = vert, [shader_index][1] = frag
// ─────────────────────────────────────────────────────────────────────────────
static const char *s_shader_paths[MAX_SHADER_PROGRAMS][2] =
{
    { "resources/shaders/mesh_0x2.vert.spv",
      "resources/shaders/textured_mesh_lighted.frag.spv" },
    { "resources/shaders/mesh_0xA.vert.spv",
      "resources/shaders/textured_mesh_lighted.frag.spv" },
    { "resources/shaders/mesh_0x12.vert.spv",
      "resources/shaders/textured_mesh_lighted.frag.spv" },
    { "resources/shaders/untextured_coloured_sprite.vert.spv",
      "resources/shaders/untextured_coloured_sprite.frag.spv" },
    { "resources/shaders/bounding_box.vert.spv",
      "resources/shaders/untextured_coloured_sprite.frag.spv" },
    { "resources/shaders/sprite.vert.spv",
      "resources/shaders/sprite.frag.spv" }
};

// Number of samplers per fragment shader index
static const Uint32 s_frag_samplers[MAX_SHADER_PROGRAMS] =
    { 1, 1, 1, 0, 0, 1 };

// Number of uniform buffers per vertex shader index
static const Uint32 s_vert_ubos[MAX_SHADER_PROGRAMS] =
    { 1, 1, 1, 0, 1, 0 };

// Number of uniform buffers per fragment shader index
// (LightBlock = binding 0, FragMiscUniforms = binding 1)
static const Uint32 s_frag_ubos[MAX_SHADER_PROGRAMS] =
    { 2, 2, 2, 0, 0, 0 };

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static SDL_GPUShader *load_spirv_shader(SDL_GPUDevice     *device,
                                        const char        *path,
                                        SDL_GPUShaderStage stage,
                                        Uint32             num_samplers,
                                        Uint32             num_uniform_buffers,
                                        Uint32             num_storage_buffers)
{
    size_t code_size = 0;
    void  *code      = SDL_LoadFile(path, &code_size);

    if (!code || code_size == 0)
    {
        info_log("SDL_GPU: failed to load shader '%s': %s", path, SDL_GetError());
        SDL_free(code);
        return NULL;
    }

    SDL_ShaderCross_ShaderStage sc_stage = (stage == SDL_GPU_SHADERSTAGE_VERTEX)
        ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
        : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    SDL_ShaderCross_SPIRV_Info spirv_info = {
        .bytecode      = (const Uint8 *)code,
        .bytecode_size = (size_t)code_size,
        .entrypoint    = "main",
        .shader_stage  = sc_stage,
        .props         = 0,
    };

    SDL_ShaderCross_GraphicsShaderResourceInfo resource_info = {
        .num_samplers         = num_samplers,
        .num_storage_textures = 0,
        .num_storage_buffers  = num_storage_buffers,
        .num_uniform_buffers  = num_uniform_buffers,
    };

    SDL_GPUShader *shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
        device, &spirv_info, &resource_info, 0);

    SDL_free(code);

    if (!shader)
    {
        info_log("SDL_GPU: failed to compile shader '%s': %s", path, SDL_GetError());
    }

    return shader;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

int MikuPan_InitShaders(SDL_GPUDevice *device)
{
    if (!SDL_ShaderCross_Init())
    {
        info_log("SDL_ShaderCross_Init failed: %s", SDL_GetError());
        return -1;
    }

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        g_vertex_shaders[i] = load_spirv_shader(
            device,
            s_shader_paths[i][0],
            SDL_GPU_SHADERSTAGE_VERTEX,
            /* samplers */ 0,
            s_vert_ubos[i],
            /* storage buffers */ 0);

        g_fragment_shaders[i] = load_spirv_shader(
            device,
            s_shader_paths[i][1],
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            s_frag_samplers[i],
            s_frag_ubos[i],
            /* storage buffers */ 0);

        if (!g_vertex_shaders[i] || !g_fragment_shaders[i])
        {
            info_log("SDL_GPU: shader pair %d failed to load", i);
            return -1;
        }
    }

    info_log("SDL_GPU: loaded %d shader pairs", MAX_SHADER_PROGRAMS);
    return 0;
}

void MikuPan_DestroyShaders(SDL_GPUDevice *device)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        if (g_vertex_shaders[i])
        {
            SDL_ReleaseGPUShader(device, g_vertex_shaders[i]);
            g_vertex_shaders[i] = NULL;
        }
        if (g_fragment_shaders[i])
        {
            SDL_ReleaseGPUShader(device, g_fragment_shaders[i]);
            g_fragment_shaders[i] = NULL;
        }
    }
    SDL_ShaderCross_Quit();
}

u_int MikuPan_SetCurrentShaderProgram(int shader_program)
{
    if (shader_program >= 0 && shader_program < MAX_SHADER_PROGRAMS)
    {
        g_current_shader_type = shader_program;
    }
    return (u_int) g_current_shader_type;
}

u_int MikuPan_GetCurrentShaderProgram(void)
{
    return (u_int) g_current_shader_type;
}

// ─────────────────────────────────────────────────────────────────────────────
// Uniform state setters
// These keep parity with the old glUniform* wrappers so game-code call sites
// remain unchanged.  The actual GPU push happens in MikuPan_EndFrame().
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_SetUniformMatrix4fvToAllShaders(float *mat, char *name)
{
    if (strcmp(name, "model") == 0)
    {
        memcpy(g_vert_uniforms.model,      mat, sizeof(float) * 16);
        memcpy(g_bb_uniforms.model,        mat, sizeof(float) * 16);
    }
    else if (strcmp(name, "view") == 0)
    {
        memcpy(g_vert_uniforms.view,       mat, sizeof(float) * 16);
        memcpy(g_bb_uniforms.view,         mat, sizeof(float) * 16);
    }
    else if (strcmp(name, "projection") == 0)
    {
        memcpy(g_vert_uniforms.projection, mat, sizeof(float) * 16);
        memcpy(g_bb_uniforms.projection,   mat, sizeof(float) * 16);
    }
}

void MikuPan_SetUniform4fvToAllShaders(float *vector, char *name)
{
    if (strcmp(name, "uFog") == 0)
    {
        memcpy(g_frag_misc_uniforms.uFog,      vector, sizeof(float) * 4);
    }
    else if (strcmp(name, "uFogColor") == 0)
    {
        memcpy(g_frag_misc_uniforms.uFogColor, vector, sizeof(float) * 4);
    }
}

void MikuPan_SetUniform4fvToCurrentShader(float *vector, char *name)
{
    if (strcmp(name, "uColor") == 0)
    {
        memcpy(g_bb_uniforms.uColor, vector, sizeof(float) * 4);
    }
    // Delegate other 4fv uniforms to the "all shaders" path
    MikuPan_SetUniform4fvToAllShaders(vector, name);
}

void MikuPan_SetUniform1iToAllShaders(int value, char *name)
{
    if (strcmp(name, "renderNormals") == 0)
    {
        g_frag_misc_uniforms.renderNormals = value;
    }
}

void MikuPan_SetUniform1fToAllShaders(float value, char *name)
{
    if (strcmp(name, "uColorScale") == 0)
    {
        g_frag_misc_uniforms.uColorScale = value;
    }
}
