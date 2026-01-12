#ifndef MIKUPAN_MIKUPAN_SHADER_H
#define MIKUPAN_MIKUPAN_SHADER_H
#include "typedefs.h"

enum ShaderPrograms
{
    DEFAULT_SHADER,
    MAX_SHADER_PROGRAMS
};

extern u_int shader_list[MAX_SHADER_PROGRAMS];

int MikuPan_InitShaders();
void MikuPan_BackUpCurrentShaderProgram();
void MikuPan_RestoreCurrentShaderProgram();
void MikuPan_SetCurrentShaderProgram(int shader_program);
void MikuPan_SetShaderProgramWithBackup(int shader_program);
u_int MikuPan_GetCurrentShaderProgram();

#endif//MIKUPAN_MIKUPAN_SHADER_H
