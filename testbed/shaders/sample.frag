/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#version 450

layout(location = 0) in vec2 fragUV;
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(tex, fragUV);
}
