#version 330 core

in vec2 vUV;

uniform sampler2D uScene;
uniform float     uTime;
uniform float     uResX;
uniform float     uResY;

out vec4 fragColor;

float hash(float n) { return fract(sin(n) * 43758.5453123); }
float hash2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float glitchOffset(float y, float time) {
    float band      = floor(y * 60.0);
    float bandNoise = hash(band + floor(time * 12.0) * 100.0);
    float active    = step(0.97, bandNoise);
    float bigHit    = step(0.995, hash(band * 7.3 + time * 3.0));
    float offset    = 0.0;
    offset += active * (hash(band * 3.1 + time) - 0.5) * 0.02;
    offset += bigHit * (hash(band * 5.7 + time) - 0.5) * 0.05;
    return offset;
}

vec3 chromaShift(vec2 uv, float strength) {
    float r = texture(uScene, uv + vec2( strength, 0.0)).r;
    float g = texture(uScene, uv                       ).g;
    float b = texture(uScene, uv - vec2( strength, 0.0)).b;
    return vec3(r, g, b);
}

void main() {
    vec2 uv = vUV;

    float gOff = glitchOffset(uv.y, uTime);
    uv.x += gOff;
    uv.x = fract(uv.x);

    float chromaStr = 0.002 + abs(gOff) * 1.2;
    vec3 color = chromaShift(uv, chromaStr);

    float scanline = 0.88 + 0.12 * sin(vUV.y * uResY * 3.14159);
    color *= scanline;

    float rollY   = fract(uTime * 0.07);
    float rollDim = 1.0 - 0.08 * smoothstep(0.0, 0.04, abs(vUV.y - rollY));
    color *= rollDim;

    float grain = hash2(vUV + fract(uTime * 100.0)) * 0.03 - 0.015;
    color += grain;

    vec2  center   = vUV - 0.5;
    float vignette = 1.0 - dot(center, center) * 0.8;
    vignette       = clamp(vignette, 0.0, 1.0);
    color         *= vignette;

    color *= 1.0;

    float pulse = 0.96 + 0.04 * sin(uTime * 0.7);
    color *= pulse;

    fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
