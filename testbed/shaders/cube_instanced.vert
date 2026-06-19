#version 450

// Per-cube constants come from a dynamic uniform buffer: one buffer holds every cube's
// transform + color, and each draw binds it with a per-cube byte offset (the testbed
// sets that offset via CommandBuffer::bindDescriptorSet(.., dynamicOffset)).
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 0) uniform Object
{
    mat4 mvp;
    mat4 model;
    vec4 color;
} obj;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;

void main()
{
    gl_Position = obj.mvp * vec4(inPos, 1.0);
    fragNormal = mat3(obj.model) * inNormal;
    fragColor = obj.color.rgb;
}
