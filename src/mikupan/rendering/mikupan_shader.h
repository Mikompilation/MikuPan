#ifndef MIKUPAN_MIKUPAN_SHADER_H
#define MIKUPAN_MIKUPAN_SHADER_H

#include "typedefs.h"
#include <SDL3/SDL_gpu.h>

// ─────────────────────────────────────────────────────────────────────────────
// Shader program identifiers
// ─────────────────────────────────────────────────────────────────────────────
enum ShaderPrograms
{
    MESH_0x2_SHADER,
    MESH_0xA_SHADER,
    MESH_0x12_SHADER,
    UNTEXTURED_COLOURED_SPRITE_SHADER,
    BOUNDING_BOX_SHADER,
    SPRITE_SHADER,
    MAX_SHADER_PROGRAMS
};

// ─────────────────────────────────────────────────────────────────────────────
// Vertex uniform block (set=3, binding=0 in mesh shaders)
// Matches MeshUniforms and BBUniforms in the GLSL shaders.
// ─────────────────────────────────────────────────────────────────────────────
typedef struct
{
    float model[16];
    float view[16];
    float projection[16];
} MikuPan_MeshVertUniforms;

// Extended version used by the bounding-box shader (adds uColor)
typedef struct
{
    float model[16];
    float view[16];
    float projection[16];
    float uColor[4];
} MikuPan_BBVertUniforms;

// ─────────────────────────────────────────────────────────────────────────────
// Fragment misc uniform block (set=7, binding=1 in mesh shaders)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct
{
    int   renderNormals;
    float uColorScale;
    float _pad[2];
    float uFog[4];      // x=min, y=max, z=base, w=scale
    float uFogColor[4];
} MikuPan_FragMiscUniforms;

// ─────────────────────────────────────────────────────────────────────────────
// Global shader state (uniform values tracked between game-logic render calls)
// ─────────────────────────────────────────────────────────────────────────────
extern MikuPan_MeshVertUniforms  g_vert_uniforms;
extern MikuPan_BBVertUniforms    g_bb_uniforms;
extern MikuPan_FragMiscUniforms  g_frag_misc_uniforms;

// ─────────────────────────────────────────────────────────────────────────────
// SDL_GPU shader objects (one vert + one frag per logical program pair)
// ─────────────────────────────────────────────────────────────────────────────
extern SDL_GPUShader *g_vertex_shaders[MAX_SHADER_PROGRAMS];
extern SDL_GPUShader *g_fragment_shaders[MAX_SHADER_PROGRAMS];

// Current "active" shader type (set by MikuPan_SetCurrentShaderProgram)
extern int g_current_shader_type;

// ─────────────────────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────────────────────
int   MikuPan_InitShaders(SDL_GPUDevice *device);
void  MikuPan_DestroyShaders(SDL_GPUDevice *device);

// Sets the active shader type index; returns the index (kept for caller compat)
u_int MikuPan_SetCurrentShaderProgram(int shader_program);
u_int MikuPan_GetCurrentShaderProgram(void);

// Uniform setters – these update the global state structs that are pushed to
// the GPU as push-constants before each draw call in MikuPan_EndFrame().
void  MikuPan_SetUniformMatrix4fvToAllShaders(float *mat,    char *name);
void  MikuPan_SetUniform4fvToAllShaders      (float *vector, char *name);
void  MikuPan_SetUniform4fvToCurrentShader   (float *vector, char *name);
void  MikuPan_SetUniform1iToAllShaders       (int    value,  char *name);
void  MikuPan_SetUniform1fToAllShaders       (float  value,  char *name);

#endif /* MIKUPAN_MIKUPAN_SHADER_H */
