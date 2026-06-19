#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 n = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.6));
    float light = 0.3 + 0.7 * max(dot(n, lightDir), 0.0);
    outColor = vec4(fragColor * light, 1.0);
}
