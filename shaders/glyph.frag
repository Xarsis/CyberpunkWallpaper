#version 330 core

in vec2 vUV;
in vec4 vColor;

uniform sampler2D uAtlas;
uniform float     uTime;

out vec4 fragColor;

void main() {
    // Atlas stores glyphs as single-channel (red) alpha mask
    float alpha = texture(uAtlas, vUV).r;

    // Widen the readable range — keep most of the glyph pixels visible
    // Lower upper bound so even grey pixels in the atlas pass through
    alpha = smoothstep(0.02, 0.35, alpha);

    // Subtle shimmer
    float shimmer = 0.92 + 0.08 * sin(uTime * 8.0 + vUV.y * 30.0);

    fragColor = vec4(vColor.rgb * shimmer, vColor.a * alpha);
}
