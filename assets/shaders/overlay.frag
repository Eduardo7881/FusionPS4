#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(binding = 0) uniform sampler2D fontAtlas;
layout(location = 0) out vec4 outColor;
void main() {
    vec4 tex = texture(fontAtlas, inUV);
    outColor = tex.a * inColor;
}
