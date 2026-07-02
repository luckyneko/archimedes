/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragTint;
layout(location = 2) flat in int fragTex;

// A 3-element sampler array (descriptor array); the sprite picks one by index.
layout(set = 0, binding = 1) uniform sampler2D tex[3];

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(tex[fragTex], fragUV) * fragTint;
}
