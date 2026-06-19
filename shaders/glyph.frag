#version 330 core

in vec2 vUV;
in vec4 vColor;

uniform sampler2D uAtlas;
uniform float     uTime;

out vec4 fragColor;

void main() {
    float alpha = texture(uAtlas, vUV).r;
    alpha = smoothstep(0.02, 0.35, alpha);
    float shimmer = 0.80 + 0.08 * sin(uTime * 8.0 + vUV.y * 30.0);
    fragColor = vec4(vColor.rgb * shimmer, vColor.a * alpha);
}
