#version 330 core
// Shadow silhouette pass, bound on top of an existing mesh VAO through the
// shader-override mechanism in MikuPan_SetCurrentShaderProgram. Every mesh
// VAO in the engine has the position attribute at location 0, either vec3 or
// vec4, so reading vec3 picks up xyz cleanly from either format.
layout (location = 0) in vec3 aPos;

// Shared with the regular mesh shaders. During the shadow pass, this is
// shadow_world_clip_view * model from the PS2 orthographic shadow camera.
uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(aPos, 1.0);
}
