#version 330 core

// Stub silhouette pass — until the real DrawShadowModel iteration is wired
// through MikuPan_RenderMeshType*, the caster is approximated as an ellipse
// fitted into the shadow camera's frustum. SetShadowCamera in shadow.c
// already sized that frustum to the caster's bounding box, so an ellipse
// tangent to the FBO border is a reasonable approximation.
//
// The vertex shader is sprite.vert which puts the fullscreen quad in clip
// space. We re-derive the [-1, 1] coord here from gl_FragCoord normalized to
// the FBO size so the ellipse stays centered regardless of vertex layout.

uniform vec2  uShadowSize;     ///< FBO width/height in pixels
uniform float uShadowDarkness; ///< 0..1; 1 = full black, 0 = transparent

out vec4 FragColor;

void main()
{
    // -1..1 across the FBO.
    vec2 uv = (gl_FragCoord.xy / uShadowSize) * 2.0 - 1.0;
    float r = length(uv);

    // Soft-edge ellipse: full opacity in the inner 70%, fade to 0 by edge.
    float a = 1.0 - smoothstep(0.7, 1.0, r);

    if (a <= 0.0) discard;
    FragColor = vec4(0.0, 0.0, 0.0, a * uShadowDarkness);
}
