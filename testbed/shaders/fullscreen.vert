/*
 *  Created by LuckyNeko on 19/06/2026.
 *  Copyright 2026 LuckyNeko
 *
 *  Distributed under the MIT Software License
 *  (See accompanying file LICENSE)
 */

#version 450

// A full-screen triangle from gl_VertexIndex (no vertex buffer), with a 0..1 UV. Pair
// with a fragment shader that samples something. Draw with draw(3).
layout(location = 0) out vec2 fragUV;

void main()
{
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    fragUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
