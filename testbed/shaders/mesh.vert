#version 450

// Programmable vertex pulling: the geometry is NOT a vertex buffer. The shared mesh
// lives in a storage buffer that the main thread rewrites every frame; both windows'
// vertex shaders read it here via gl_VertexIndex (the index buffer still drives the
// draw). std430 packs each vertex as three vec4s (48 bytes), matching the CPU struct.
struct Vertex
{
    vec4 pos;    // xyz used
    vec4 normal; // xyz used
    vec4 uv;     // xy used
};

layout(set = 0, binding = 0) uniform Camera
{
    mat4 mvp;
    mat4 model;
} cam;

layout(std430, set = 0, binding = 1) readonly buffer Mesh
{
    Vertex verts[];
};

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

void main()
{
    Vertex v = verts[gl_VertexIndex];
    gl_Position = cam.mvp * vec4(v.pos.xyz, 1.0);
    fragNormal = mat3(cam.model) * v.normal.xyz;
    fragUV = v.uv.xy;
}
