#version 330 core
// Constant-alpha output. The shadow FBO is single-channel R8 — only .r
// reaches the texture. Strength of the resulting darkening is controlled by
// uShadowStrength on the receiver side, so this just writes 1.0 to mark
// "this fragment was occupied by the caster".

out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
