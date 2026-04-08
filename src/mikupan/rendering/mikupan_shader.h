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
    MAX_SHADER_PROGRAMS
};

extern u_int shader_list[MAX_SHADER_PROGRAMS];

int MikuPan_InitShaders();
u_int MikuPan_SetCurrentShaderProgram(int shader_program);
u_int MikuPan_GetCurrentShaderProgram();
void MikuPan_SetUniformMatrix4fvToAllShaders(float* mat, char* name);
void MikuPan_SetUniform4fvToAllShaders(float* vector, char* name);
void MikuPan_SetUniform4fvToCurrentShader(float* vector, char* name);
void MikuPan_SetUniform1iToAllShaders(int value, char* name);
void MikuPan_SetUniform1fToAllShaders(float value, char *name);

#endif//MIKUPAN_MIKUPAN_SHADER_H
