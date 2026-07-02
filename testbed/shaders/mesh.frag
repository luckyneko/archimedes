/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;

layout(set = 0, binding = 2) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 n = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.5));
    float diffuse = max(dot(n, lightDir), 0.0);
    float light = 0.3 + 0.7 * diffuse; // ambient + diffuse
    vec3 albedo = texture(tex, fragUV).rgb;
    outColor = vec4(albedo * light, 1.0);
}
