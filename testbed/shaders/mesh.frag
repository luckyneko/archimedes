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

// Exact sRGB transfer functions (IEC 61966-2-1), not the pow(2.2) approximation.
// The swapchain is non-encoding (UNORM view), so this shader owns its encoding:
// light must happen in linear space, then the result is encoded for display.
vec3 srgbToLinear(vec3 c)
{
    bvec3 cutoff = lessThanEqual(c, vec3(0.04045));
    vec3 lower = c / 12.92;
    vec3 higher = pow((c + 0.055) / 1.055, vec3(2.4));
    return mix(higher, lower, vec3(cutoff));
}

vec3 linearToSrgb(vec3 c)
{
    bvec3 cutoff = lessThanEqual(c, vec3(0.0031308));
    vec3 lower = c * 12.92;
    vec3 higher = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(higher, lower, vec3(cutoff));
}

void main()
{
    vec3 n = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.5));
    float diffuse = max(dot(n, lightDir), 0.0);
    float light = 0.3 + 0.7 * diffuse; // ambient + diffuse (a linear intensity)

    // The texture stores sRGB-encoded bytes; decode to linear before lighting.
    vec3 albedo = srgbToLinear(texture(tex, fragUV).rgb);
    vec3 lit = albedo * light;

    // Encode back to sRGB for the non-encoding swapchain (alpha is not colour, left linear).
    outColor = vec4(linearToSrgb(lit), 1.0);
}
