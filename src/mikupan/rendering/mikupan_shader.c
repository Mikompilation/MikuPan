#include "mikupan_shader.h"

#include "glad/gl.h"
#include "mikupan/mikupan_file_c.h"
#include <stdlib.h>

GLuint current_program = 0;
GLuint backup_current_program = 0;
u_int shader_list[MAX_SHADER_PROGRAMS] = {0};

const char* shader_file_name[MAX_SHADER_PROGRAMS][2] = {
    {"./shaders/default.vert", "./shaders/default.frag"},
    {"./shaders/mesh_0x12.vert", "./shaders/mesh_0x12.frag"},
    {"./shaders/ui_sprite.vert", "./shaders/ui_sprite.frag"},
    {"./shaders/untextured_sprite.vert", "./shaders/untextured_sprite.frag"},
    {"./shaders/simple_textured.vert", "./shaders/simple_textured.frag"},
    {"./shaders/bounding_box.vert", "./shaders/bounding_box.frag"},
    {"./shaders/sprite_3D.vert", "./shaders/sprite_3D.frag"}
};

int MikuPan_InitShaders()
{
    for (int i = 0 ; i < MAX_SHADER_PROGRAMS ; i++)
    {
        const char* vertex_shader_filename = shader_file_name[i][0];
        u_int shader_file_size = MikuPan_GetFileSize(vertex_shader_filename) + 1;

        char* vertex_shader_buffer = malloc(shader_file_size);
        vertex_shader_buffer[shader_file_size - 1] = 0;

        MikuPan_ReadFullFile(vertex_shader_filename, vertex_shader_buffer);

        GLuint vertexShader = glad_glCreateShader(GL_VERTEX_SHADER);
        glad_glShaderSource(vertexShader, 1, (const GLchar *const *)&vertex_shader_buffer, NULL);

        // Compile the Vertex Shader into machine code
        glad_glCompileShader(vertexShader);

        // Create Fragment Shader Object and get its reference
        GLuint fragmentShader = glad_glCreateShader(GL_FRAGMENT_SHADER);

        const char* frag_shader_filename = shader_file_name[i][1];
        shader_file_size = MikuPan_GetFileSize(frag_shader_filename) + 1;

        char* frag_buffer = malloc(shader_file_size);
        frag_buffer[shader_file_size - 1] = 0;
        MikuPan_ReadFullFile(frag_shader_filename, frag_buffer);

        // Attach Fragment Shader source to the Fragment Shader Object
        glad_glShaderSource(fragmentShader, 1, (const GLchar *const *)&frag_buffer, NULL);

        // Compile the Vertex Shader into machine code
        glad_glCompileShader(fragmentShader);

        // Create Shader Program Object and get its reference
        current_program = glad_glCreateProgram();
        shader_list[i] = current_program;

        // Attach the Vertex and Fragment Shaders to the Shader Program
        glad_glAttachShader(current_program, vertexShader);
        glad_glAttachShader(current_program, fragmentShader);

        // Wrap-up/Link all the shaders together into the Shader Program
        glad_glLinkProgram(current_program);

        // Delete the now useless Vertex and Fragment Shader objects
        glad_glDeleteShader(vertexShader);
        glad_glDeleteShader(fragmentShader);

        free(vertex_shader_buffer);
        free(frag_buffer);
    }

    // Tell OpenGL which Shader Program we want to use
    glad_glUseProgram(current_program);

    return 0;
}

void MikuPan_BackUpCurrentShaderProgram()
{
    glad_glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&backup_current_program);
}

void MikuPan_RestoreCurrentShaderProgram()
{
    glad_glUseProgram(backup_current_program);
}

void MikuPan_SetCurrentShaderProgram(int shader_program)
{
    if (shader_program >= MAX_SHADER_PROGRAMS)
    {
        return;
    }

    current_program = shader_list[shader_program];

    glad_glUseProgram(current_program);
}

void MikuPan_SetShaderProgramWithBackup(int shader_program)
{
    MikuPan_BackUpCurrentShaderProgram();
    MikuPan_SetCurrentShaderProgram(shader_program);
}

u_int MikuPan_GetCurrentShaderProgram()
{
    return current_program;
}