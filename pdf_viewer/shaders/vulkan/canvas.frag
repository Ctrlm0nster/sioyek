#version 440
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 drawing_color;
layout(location = 0) out vec4 color;
layout(std140, binding = 0) uniform Uniforms {
    mat4 correction;
    mat4 transform;
    vec4 tint;
    vec4 params;
};
layout(binding = 1) uniform sampler2D page_texture;

// Color conversion retained from upstream Sioyek's dark_mode.fragment:
// http://gamedev.stackexchange.com/questions/59797/glsl-shader-change-hue-saturation-brightness
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
void main() {
    int mode = int(params.x);
    if (mode == 1) color = vec4(texture(page_texture, uv).rgb, 1.0);
    else if (mode == 2) {
        vec3 inverted = (0.5 - texture(page_texture, uv).rgb) * params.y + 0.5;
        vec3 hsv = rgb2hsv(inverted);
        color = vec4(hsv2rgb(vec3(mod(hsv.x + 0.5, 1.0), hsv.yz)), 1.0);
    } else if (mode == 3) {
        color = vec4(clamp((transform * vec4(texture(page_texture, uv).rgb, 1.0)).rgb, 0.0, 1.0), 1.0);
    } else if (mode == 4) {
        int checker = (int(uv.x / 0.1) + int(uv.y / 0.1)) % 2;
        color = vec4(vec3(checker == 0 ? 1.0 : 0.1), 1.0);
    } else if (mode == 7) color = vec4(1.0 - tint.rgb, tint.a);
    else if (mode == 8) color = vec4(tint.rgb, 1.0);
    else if (mode == 9) color = vec4(0.0);
    else if (mode == 11) color = drawing_color;
    else if (mode == 12) {
        vec2 p = (uv - 0.5) * 2.0;
        float radius = dot(p, p);
        float delta = fwidth(radius);
        color = vec4(drawing_color.rgb, 1.0 - smoothstep(1.0 - delta, 1.0 + delta, radius));
    } else if (mode == 13) color = texture(page_texture, uv);
    else color = tint;
}
