#version 440
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec4 vertex_color;
layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 drawing_color;
layout(std140, binding = 0) uniform Uniforms {
    mat4 correction;
    mat4 transform;
    vec4 tint;
    vec4 params;
};
void main() {
    uv = texcoord;
    drawing_color = vertex_color;
    gl_Position = correction * vec4(position, params.w, 1.0);
}
