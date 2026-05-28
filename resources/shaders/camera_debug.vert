#version 330 core
layout (location = 0) in vec4 aPos;

uniform mat4 uWorldClipView;
uniform vec4 uColor;

out vec4 outColor;

void main()
{
    vec4 clip = uWorldClipView * aPos;

    // SgCAMERA.wcv is the PS2 world-clip-view matrix. Its clip Z is 0..W,
    // while OpenGL expects -W..W, so remap Z without touching X/Y/W.
    gl_Position = vec4(clip.x, clip.y, clip.z * 2.0 - clip.w, clip.w);
    outColor = uColor;
}
