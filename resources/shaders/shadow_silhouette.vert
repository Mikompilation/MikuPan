#version 330 core
// Shadow silhouette pass — bound on top of an existing mesh VAO via the
// shader-override mechanism in MikuPan_SetCurrentShaderProgram. Every mesh
// VAO in the engine has the position attribute at location 0 (vec3 or vec4
// depending on the layout), so reading vec3 here picks up xyz cleanly from
// either format.
layout (location = 0) in vec3 aPos;

// Shared with the regular mesh shaders. The shadow pass arranges for `mvp`
// to be `shadow_proj * shadow_view * model` — see MikuPan_BeginShadowPass +
// MikuPan_SetupCamera applied with the shadow camera right before draw.
uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vec4(aPos, 1.0);
}
