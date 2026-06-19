#version 450

// Each sprite's rect/tint/texture-index comes from a dynamic uniform (one buffer, a
// per-sprite offset). The quad is built from gl_VertexIndex (triangle strip, draw(4)) —
// no vertex buffer. rect = (centerX, centerY, halfW, halfH) in NDC.
layout(set = 0, binding = 0) uniform Sprite
{
    vec4 rect;
    vec4 tint;
    ivec4 misc; // x = texture index
} s;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragTint;
layout(location = 2) flat out int fragTex;

void main()
{
    vec2 corner = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
    fragUV = corner;
    vec2 pos = s.rect.xy + (corner * 2.0 - 1.0) * s.rect.zw;
    gl_Position = vec4(pos, 0.0, 1.0);
    fragTint = s.tint;
    fragTex = s.misc.x;
}
