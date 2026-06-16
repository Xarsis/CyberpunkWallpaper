#version 330 core

// Per-vertex (unit quad)
layout(location = 0) in vec2 aQuadPos;   // 0..1
layout(location = 1) in vec2 aQuadUV;

// Per-instance
layout(location = 2) in vec2  iPos;          // screen pixel origin
layout(location = 3) in vec2  iSize;         // pixel size of glyph
layout(location = 4) in vec2  iUVMin;
layout(location = 5) in vec2  iUVMax;
layout(location = 6) in vec4  iColor;
layout(location = 7) in float iGlitchOffset; // pixel horizontal jitter

uniform mat4 uOrtho;
uniform float uTime;

out vec2 vUV;
out vec4 vColor;

void main() {
    // Map unit quad to screen position + apply glitch jitter
    vec2 pixelPos = iPos + aQuadPos * iSize;
    pixelPos.x += iGlitchOffset * iSize.x;

    gl_Position = uOrtho * vec4(pixelPos, 0.0, 1.0);

    // Remap UVs into atlas region
    vUV    = iUVMin + aQuadUV * (iUVMax - iUVMin);
    vColor = iColor;
}
