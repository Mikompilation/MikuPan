#ifndef MIKUPAN_MIKUPAN_SHADER_H
#define MIKUPAN_MIKUPAN_SHADER_H
#include "typedefs.h"

enum ShaderPrograms
{
    MESH_0x2_SHADER,
    MESH_0xA_SHADER,
    MESH_0x12_SHADER,
    UNTEXTURED_COLOURED_SPRITE_SHADER,
    BOUNDING_BOX_SHADER,
    SPRITE_SHADER,
    NORMALS_0x12_SHADER,
    NORMALS_0x2_SHADER,
    POSTPROCESS_SHADER,
    SHADOW_BLOB_SHADER,       ///< debug fallback — fitted ellipse in the shadow FBO
    SHADOW_SILHOUETTE_SHADER, ///< real per-mesh silhouette into the shadow FBO
    CAMERA_DEBUG_SHADER,
    MAX_SHADER_PROGRAMS
};

enum UniformBuffers
{
    LightBlock    = 0,
    MaterialBlock = 1
};

extern u_int shader_list[MAX_SHADER_PROGRAMS];

int MikuPan_InitShaders();
u_int MikuPan_SetCurrentShaderProgram(int shader_program);
u_int MikuPan_GetCurrentShaderProgram();
void MikuPan_SetUniformMatrix4fvToAllShaders(float* mat, char* name);
void MikuPan_SetUniformMatrix3fvToAllShaders(float* mat, char* name);
void MikuPan_SetUniformMatrix4fvToCurrentShader(float* mat, char* name);
void MikuPan_SetUniformMatrix3fvToCurrentShader(float* mat, char* name);
void MikuPan_SetUniform4fvToAllShaders(float* vector, char* name);
void MikuPan_SetUniform4fvToCurrentShader(float* vector, char* name);
void MikuPan_SetUniform1iToAllShaders(int value, char* name);
void MikuPan_SetUniform1iToCurrentShader(int value, char* name);
void MikuPan_SetUniform1fToAllShaders(float value, char* name);
void MikuPan_SetUniform1fToCurrentShader(float value, char* name);
void MikuPan_SetUniform2fToCurrentShader(float x, float y, char* name);
void MikuPan_ResetShaderCache(void);
unsigned int MikuPan_GetShaderGeneration(void);

/// Per-pass shader override — when set to a valid index, every call to
/// MikuPan_SetCurrentShaderProgram routes through this shader instead. Used
/// by the shadow pass to redirect every mesh-type renderer onto the
/// silhouette shader without touching their call sites. Pass -1 to clear.
void MikuPan_SetShaderOverride(int shader_index);
int  MikuPan_GetShaderOverride(void);

/// Hot reload — recompile + relink the shader at index `idx` from its
/// on-disk source files and swap the live program atomically. On compile or
/// link failure, the existing program is kept and an error message is
/// written into `err_buf` (caller-provided, optional). Returns 0 on success,
/// -1 on any failure.
int MikuPan_ReloadShader(int idx, char *err_buf, int err_buf_size);

/// Reload every shader in turn. Returns 0 if all succeeded, -1 if any failed
/// (the others are still applied). The error from the *first* failure is
/// written to `err_buf`.
int MikuPan_ReloadAllShaders(char *err_buf, int err_buf_size);

/// UI-friendly name and source-file paths for the shader at `idx`. Names are
/// stable strings (the enum identifier minus the _SHADER suffix). Source
/// path getters return NULL when the shader has no geometry stage.
const char *MikuPan_GetShaderName(int idx);
const char *MikuPan_GetShaderVertSource(int idx);
const char *MikuPan_GetShaderGeomSource(int idx);
const char *MikuPan_GetShaderFragSource(int idx);

#endif//MIKUPAN_MIKUPAN_SHADER_H
