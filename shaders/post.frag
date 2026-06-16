#version 330 core

in vec2 vUV;

uniform sampler2D uScene;
uniform float     uTime;
uniform float     uResX;
uniform float     uResY;

out vec4 fragColor;

// ─── Hash / noise helpers ──────────────────────────────────────────────────────
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float hash2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

// ─── Glitch strip horizontal offset ───────────────────────────────────────────
float glitchOffset(float y, float time) {
    float band      = floor(y * 60.0);
    float bandNoise = hash(band + floor(time * 12.0) * 100.0);

    float active = step(0.94, bandNoise);           // ~6% of bands glitch (was 12%)
    float bigHit = step(0.985, hash(band * 7.3 + time * 3.0)); // rarer wide shift

    float offset = 0.0;
    offset += active * (hash(band * 3.1 + time) - 0.5) * 0.03;
    offset += bigHit * (hash(band * 5.7 + time) - 0.5) * 0.08;

    return offset;
}

// ─── Chromatic aberration ──────────────────────────────────────────────────────
vec3 chromaShift(vec2 uv, float strength) {
    float r = texture(uScene, uv + vec2( strength, 0.0)).r;
    float g = texture(uScene, uv                       ).g;
    float b = texture(uScene, uv - vec2( strength, 0.0)).b;
    return vec3(r, g, b);
}

// ─── Main ──────────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = vUV;

    // ── 1. Glitch strip offset ─────────────────────────────────────────────────
    float gOff = glitchOffset(uv.y, uTime);
    uv.x += gOff;
    uv.x = fract(uv.x);

    // ── 2. Chromatic aberration ────────────────────────────────────────────────
    float chromaStr = 0.002 + abs(gOff) * 1.2;
    vec3 color = chromaShift(uv, chromaStr);

    // ── 3. Scanlines — very subtle, just a hint ────────────────────────────────
    // Use pixel-frequency sine so lines are 1px wide at native res
    float scanline = 0.88 + 0.12 * sin(vUV.y * uResY * 3.14159);
    color *= scanline;

    // ── 4. Scanline roll ───────────────────────────────────────────────────────
    float rollY   = fract(uTime * 0.07);
    float rollDim = 1.0 - 0.08 * smoothstep(0.0, 0.04, abs(vUV.y - rollY));
    color *= rollDim;

    // ── 5. Film grain ──────────────────────────────────────────────────────────
    float grain = hash2(vUV + fract(uTime * 100.0)) * 0.03 - 0.015;
    color += grain;

    // ── 6. Vignette — gentle, only darkens edges ───────────────────────────────
    vec2  center   = vUV - 0.5;
    float vignette = 1.0 - dot(center, center) * 0.8; // was 1.8 — much gentler
    vignette       = clamp(vignette, 0.0, 1.0);
    color         *= vignette;

    // ── 7. Brightness boost to compensate for stacked multipliers ─────────────
    color *= 1.4;

    // ── 8. Subtle pulse ───────────────────────────────────────────────────────
    float pulse = 0.96 + 0.04 * sin(uTime * 0.7);
    color *= pulse;

    // ── 9. Clamp and output ───────────────────────────────────────────────────
    fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
